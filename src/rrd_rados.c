#include "rrd_rados.h"

rrd_rados_t* rrd_rados_open(const char *oid) {
    int err;
    rrd_rados_t *rrd_rados;

    rrd_rados = (rrd_rados_t*)malloc(sizeof(rrd_rados_t));
    if (rrd_rados == NULL) {
      rrd_set_error("allocating rrd_rados descriptor");
      return NULL;
    }

    memset(rrd_rados, 0, sizeof(rrd_rados_t));
    rrd_rados->oid = oid;

    const char *ceph_id = getenv("CEPH_ID");
    if (!ceph_id) ceph_id = "admin";
    err = rados_create(&rrd_rados->cluster, ceph_id);
    if (err < 0) {
        rrd_set_error("cannot create cluster handle: %s", strerror(-err));
        goto err;
    }

    err = rados_conf_read_file(rrd_rados->cluster, "/etc/ceph/ceph.conf");
    if (err < 0) {
        rrd_set_error("cannot read config file: %s", strerror(-err));
        goto err;
    }

    err = rados_connect(rrd_rados->cluster);
    if (err < 0) {
        rrd_set_error("cannot connect to cluster: %s", strerror(-err));
        goto err;
    }

    rrd_rados->write_op = rados_create_write_op();
    if (rrd_rados->write_op == NULL) {
        rrd_set_error("allocating rados_write_op_t");
        goto err;
    }

    const char *ceph_pool = getenv("CEPH_POOL");
    if (!ceph_pool) ceph_pool = "rrd";
    err = rados_ioctx_create(rrd_rados->cluster, ceph_pool, &rrd_rados->ioctx);
    if (err < 0) {
        rrd_set_error("cannot open rados pool: %s", strerror(-err));
        goto err;
    }

    return rrd_rados;
err:
    if (rrd_rados->write_op)
      rados_release_write_op(rrd_rados->write_op);
    if (rrd_rados->ioctx)
      rados_ioctx_destroy(rrd_rados->ioctx);
    if (rrd_rados->cluster)
      rados_shutdown(rrd_rados->cluster);
    free(rrd_rados);
    return NULL;
}

int rrd_rados_close(rrd_rados_t *rrd_rados) {
    rrd_rados_flush(rrd_rados);

    /* release lock on close, see rrd_rados_lock() */
    if (rrd_rados->lock) {
      rados_unlock(rrd_rados->ioctx, rrd_rados->oid, "rrdtool", "");
    }

    rados_release_write_op(rrd_rados->write_op);
    rados_ioctx_destroy(rrd_rados->ioctx);
    rados_shutdown(rrd_rados->cluster);
    free(rrd_rados);

    return 0;
}

int rrd_rados_create(const char *oid, rrd_t *rrd) {
    int err;

    rrd_rados_t *rrd_rados = rrd_rados_open(oid);
    if (rrd_rados == NULL)
      return -1;

    rados_write_op_truncate(rrd_rados->write_op, 0);

    /* See write_fh() defined in rrd_create.c */

    if (atoi(rrd->stat_head->version) < 3) {
        /* we output 3 or higher */
        strcpy(rrd->stat_head->version, "0003");
    }

    rados_write_op_append(rrd_rados->write_op, (char*)rrd->stat_head, sizeof(stat_head_t));
    rados_write_op_append(rrd_rados->write_op, (char*)rrd->ds_def, sizeof(ds_def_t) * rrd->stat_head->ds_cnt);
    rados_write_op_append(rrd_rados->write_op, (char*)rrd->rra_def, sizeof(rra_def_t) * rrd->stat_head->rra_cnt);
    rados_write_op_append(rrd_rados->write_op, (char*)rrd->live_head, sizeof(live_head_t));
    rados_write_op_append(rrd_rados->write_op, (char*)rrd->pdp_prep, sizeof(pdp_prep_t) * rrd->stat_head->ds_cnt);
    rados_write_op_append(rrd_rados->write_op, (char*)rrd->cdp_prep,
                    sizeof(cdp_prep_t) * rrd->stat_head->rra_cnt * rrd->stat_head->ds_cnt);
    rados_write_op_append(rrd_rados->write_op, (char*)rrd->rra_ptr, sizeof(rra_ptr_t) * rrd->stat_head->rra_cnt);

    /* calculate the number of rrd_values to dump */
    int rra_offset = 0;
    for (unsigned int i = 0; i < rrd->stat_head->rra_cnt; i++) {
        unsigned long num_rows = rrd->rra_def[i].row_cnt;
        unsigned long ds_cnt = rrd->stat_head->ds_cnt;
        if (num_rows > 0){
            rados_write_op_append(rrd_rados->write_op, (char*)(rrd->rrd_value + rra_offset * ds_cnt),
                                  sizeof(rrd_value_t) * num_rows * ds_cnt);

            rra_offset += num_rows;
        }
    }

    err = rrd_rados_flush(rrd_rados);
    if (err < 0)
        rrd_set_error("rados flush: %s", strerror(-err));

    rrd_rados_close(rrd_rados);

    return err;
}

size_t rrd_rados_read(rrd_rados_t *rrd_rados, void *data, size_t len, uint64_t offset) {
    int ret;

    ret = rados_read(rrd_rados->ioctx, rrd_rados->oid, data, len, offset);

    if (ret < 0)
        rrd_set_error("rados read: %s", strerror(-ret));

    return ret;
}

size_t rrd_rados_write(rrd_rados_t *rrd_rados, const void *data, size_t len, uint64_t offset) {
    /* writes are queued in rados write_op and written atomically on
       close or when explicitly calling flush */
    rados_write_op_write(rrd_rados->write_op, (char*)data, len, offset);

    /* writes aren't confirmed until flushed */
    return len;
}

int rrd_rados_flush(rrd_rados_t *rrd_rados) {
    return rados_write_op_operate(rrd_rados->write_op, rrd_rados->ioctx, rrd_rados->oid,
                                  NULL, LIBRADOS_OPERATION_NOFLAG);
}

int rrd_rados_lock(rrd_rados_t *rrd_rados) {
    int ret;

    /* prevent dead lock by setting a maximum lock duration */
    struct timeval tv;
    tv.tv_sec = 2; // 2 seconds
    tv.tv_usec = 0;

    ret = rados_lock_exclusive(rrd_rados->ioctx, rrd_rados->oid, "rrdtool", "", "", &tv,  0);

    if (ret < 0) {
        rrd_set_error("rados lock: %s", strerror(-ret));
    } else {
      /* set flag to instruct rrd_rados_close() to release lock */
      rrd_rados->lock = 1;
    }

    return ret;
}
