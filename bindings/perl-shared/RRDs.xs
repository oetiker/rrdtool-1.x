#ifdef __cplusplus
extern "C" {
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef __cplusplus
}
#endif

/*
 * rrd_tool.h includes config.h, but at least on Ubuntu Breezy Badger
 * 5.10 with gcc 4.0.2, the C preprocessor picks up Perl's config.h
 * which is included from the Perl includes and never reads rrdtool's
 * config.h.  Without including rrdtool's config.h, this module does
 * not compile, so include it here with an explicit path.
 *
 * Because rrdtool's config.h redefines VERSION which is originally
 * set via Perl's Makefile.PL and passed down to the C compiler's
 * command line, save the original value and reset it after the
 * includes.
 */
#define VERSION_SAVED VERSION
#undef VERSION
#ifndef WIN32
#include "rrd_config.h"
#endif
#include "rrd_tool.h"
#undef VERSION
#define VERSION VERSION_SAVED
#undef VERSION_SAVED

#define rrdcode(name) \
		argv = (char **) malloc((items+1)*sizeof(char *));\
		argv[0] = "dummy";\
		for (i = 0; i < items; i++) { \
		    STRLEN len; \
		    char *handle= SvPV(ST(i),len);\
		    /* actually copy the data to make sure possible modifications \
		       on the argv data does not backfire into perl */ \
		    argv[i+1] = (char *) malloc((strlen(handle)+1)*sizeof(char)); \
		    strcpy(argv[i+1],handle); \
 	        } \
		rrd_clear_error();\
		RETVAL=name(items+1,argv); \
		for (i=0; i < items; i++) {\
		    free(argv[i+1]);\
		} \
		free(argv);\
		\
		if (rrd_test_error()) XSRETURN_UNDEF;

#define hvs(VAL) hv_store_ent(hash, sv_2mortal(newSVpv(data->key,0)),VAL,0)

#define rrdinfocode(name) \
		/* prepare argument list */ \
		argv = (char **) malloc((items+1)*sizeof(char *)); \
		argv[0] = "dummy"; \
		for (i = 0; i < items; i++) { \
		    STRLEN len; \
		    char *handle= SvPV(ST(i),len); \
		    /* actually copy the data to make sure possible modifications \
		       on the argv data does not backfire into perl */ \
		    argv[i+1] = (char *) malloc((strlen(handle)+1)*sizeof(char)); \
		    strcpy(argv[i+1],handle); \
 	        } \
                rrd_clear_error(); \
                data=name(items+1, argv); \
                for (i=0; i < items; i++) { \
		    free(argv[i+1]); \
		} \
		free(argv); \
                if (rrd_test_error()) XSRETURN_UNDEF; \
                hash = newHV(); \
   	        save=data; \
                while (data) { \
		/* the newSV will get copied by hv so we create it as a mortal \
           to make sure it does not keep hanging round after the fact */ \
		    switch (data->type) { \
		    case RD_I_VAL: \
			if (isnan(data->value.u_val)) \
			    hvs(newSV(0)); \
			else \
			    hvs(newSVnv(data->value.u_val)); \
			break; \
			case RD_I_INT: \
			hvs(newSViv(data->value.u_int)); \
			break; \
		    case RD_I_CNT: \
			hvs(newSViv(data->value.u_cnt)); \
			break; \
		    case RD_I_STR: \
			hvs(newSVpv(data->value.u_str,0)); \
			break; \
		    case RD_I_BLO: \
			hvs(newSVpv((char *)data->value.u_blo.ptr,data->value.u_blo.size)); \
			break; \
		    } \
		    data = data->next; \
	        } \
            rrd_info_free(save); \
            RETVAL = newRV_noinc((SV*)hash);

/*
 * should not be needed if libc is linked (see ntmake.pl)
#ifdef WIN32
 #define free free
 #define malloc malloc
 #define realloc realloc
#endif
*/


static SV * rrd_fetch_cb_svptr = (SV*)NULL;

static int rrd_fetch_cb_wrapper(
    const char     *filename,  /* name of the rrd */
    enum cf_en     cf_idx, /* consolidation function */
    time_t         *start,
    time_t         *end,       /* which time frame do you want ?
                                * will be changed to represent reality */
    unsigned long  *step,      /* which stepsize do you want?
                                * will be changed to represent reality */
    unsigned long  *ds_cnt,    /* number of data sources in file */
    char           ***ds_namv, /* names of data_sources */
    rrd_value_t    **data)     /* two dimensional array containing the data */
  {
    dSP;
    HV *callHV;
    SV *retSV;
    HV *retHV;
    HE *retHE;
    AV *retAV;
    time_t new_start;
    char *cfStr = NULL;
    unsigned long i,ii;
    unsigned long rowCount = 0;
    if (!rrd_fetch_cb_svptr){
        rrd_set_error("Use RRDs::register_fetch_cb to register a fetch callback.");
        return -1;
    }

    ENTER;
    SAVETMPS;
    /* prepare the arguments */
    callHV = newHV();
    hv_store_ent(callHV, sv_2mortal(newSVpv("filename",0)),newSVpv(filename,0),0);
    switch(cf_idx){
        case CF_AVERAGE:
            cfStr = "AVERAGE";
            break;
        case CF_MINIMUM:
            cfStr = "MIN";
            break;
        case CF_MAXIMUM:
            cfStr = "MAX";
            break;
        case CF_LAST:
            cfStr = "LAST";
        default:
            break;
    }
    hv_store_ent(callHV, sv_2mortal(newSVpv("cd",0)),newSVpv(cfStr,0),0);
    hv_store_ent(callHV, sv_2mortal(newSVpv("start",0)),newSVuv(*start),0);
    hv_store_ent(callHV, sv_2mortal(newSVpv("end",0)),newSVuv(*end),0);
    hv_store_ent(callHV, sv_2mortal(newSVpv("step",0)),newSVuv(*step),0);
    PUSHMARK(SP);
    XPUSHs(newRV_noinc((SV *)callHV));
    PUTBACK;
    /* Call the Perl sub to process the callback */
    call_sv(rrd_fetch_cb_svptr , G_EVAL|G_SCALAR);
    SPAGAIN;
    /* Check the eval first */
    if (SvTRUE(ERRSV)) {
        rrd_set_error("perl callback failed: %s",SvPV_nolen(ERRSV));
        POPs; /* there is undef on top of the stack when there is an error
                 and call_sv was initiated with G_EVAL|G_SCALER */
        goto error_out;
    }
    retSV = POPs;
    if (!SvROK(retSV)){
        rrd_set_error("Expected the perl callback function to return a reference");
        goto error_out;
    }
    retHV = (HV*)SvRV(retSV);
    if (SvTYPE(retHV) != SVt_PVHV) {
        rrd_set_error("Expected the perl callback function to return a hash reference");
        goto error_out;
    }

#define loadRet(hashKey) \
    if (( retHE = hv_fetch_ent(retHV,sv_2mortal(newSVpv(hashKey,0)),0,0)) == NULL){ \
        rrd_set_error("Expected the perl callback function to return a '" hashKey "' value"); \
        goto error_out; }

    loadRet("step");
    *step = SvIV(HeVAL(retHE));
    if (*step <= 0){
        rrd_set_error("Expected the perl callback function to return a valid step value");
        goto error_out;
    }

    loadRet("start");
    new_start = SvIV(HeVAL(retHE));
    if (new_start == 0 || new_start > *start){
        rrd_set_error("Expected the perl callback function to return a start value equal or earlier than %lld but got %lld",(long long int)(*start),(long long int)(new_start));
        goto error_out;
    }

    *start = new_start;
/*    rowCount = ((*end - *start) / *step); */

    /* figure out how long things are so that we can allocate the memory */
    loadRet("data");
    retSV = HeVAL(retHE);
    if (!SvROK(retSV)){
        rrd_set_error("Expected the perl callback function to return a valid data element");
        goto error_out;
    }
    retHV = (HV*)SvRV(retSV);
    if (SvTYPE(retHV) != SVt_PVHV){
        rrd_set_error("Expected the perl callback function to return data element pointing to a hash");
        goto error_out;
    }

    *ds_cnt = hv_iterinit(retHV);

    if (((*ds_namv) = (char **) calloc( *ds_cnt , sizeof(char *))) == NULL) {
        rrd_set_error("Failed to allocate memory for ds_namev when returning from perl callback");
        goto error_out;
    }
    for (i=0;i<*ds_cnt;i++){
        char *retKey;
        I32 retKeyLen;
        HE* hash_entry;
        hash_entry = hv_iternext(retHV);
        retKey = hv_iterkey(hash_entry,&retKeyLen);
	if (strlen(retKey) >= DS_NAM_SIZE){
            rrd_set_error("Key '%s' longer than the allowed maximum of %d byte",retKey,DS_NAM_SIZE-1);
            goto error_out;
	}

        if ((((*ds_namv)[i]) = (char*)malloc(sizeof(char) * DS_NAM_SIZE)) == NULL) {
            rrd_set_error("malloc fetch ds_namv entry");
            goto error_out_free_ds_namv;
        }

        strncpy((*ds_namv)[i], retKey, DS_NAM_SIZE - 1);
        (*ds_namv)[i][DS_NAM_SIZE - 1] = '\0';
        retSV = hv_iterval(retHV,hash_entry);
        if (!SvROK(retSV)){
            rrd_set_error("Expected the perl callback function to return an array pointer for {data}->{%s}",(*ds_namv)[i]);
            goto error_out_free_ds_namv;
        }
        retAV = (AV*)SvRV(retSV);
        if (SvTYPE(retAV) != SVt_PVAV){
            rrd_set_error("Expected the perl callback function to return an array pointer for {data}->{%s}",(*ds_namv)[i]);
            goto error_out_free_ds_namv;
        }
        if (av_len(retAV)+1 > rowCount)
            rowCount = av_len(retAV)+1;
    }

    *end = *start + *step * rowCount;

    if (((*data) = (rrd_value_t*)malloc(*ds_cnt * rowCount * sizeof(rrd_value_t))) == NULL) {
        rrd_set_error("malloc fetch data area");
        goto error_out_free_ds_namv;
    }

    for (i=0;i<*ds_cnt;i++){
        retAV = (AV*)SvRV(HeVAL(hv_fetch_ent(retHV,sv_2mortal(newSVpv((*ds_namv)[i],0)),0,0)));
        for (ii=0;ii<rowCount;ii++){
            SV** valP = av_fetch(retAV,ii,0);
            SV* val = valP ? *valP : &PL_sv_undef;
            (*data)[i + ii * (*ds_cnt)] = SvNIOK(val) ? SvNVx(val) : DNAN;
        }
    }


    PUTBACK;
    FREETMPS;
    LEAVE;
    return 1;

    error_out_free_ds_namv:

    for (i = 0; i < *ds_cnt; ++i){
        if ((*ds_namv)[i]){
            free((*ds_namv)[i]);
        }
    }

    free(*ds_namv);

    error_out:
    PUTBACK;
    FREETMPS;
    LEAVE;
    return -1;

    /* prepare return data */
}


MODULE = RRDs	PACKAGE = RRDs	PREFIX = rrd_

BOOT:
#ifdef MUST_DISABLE_SIGFPE
	signal(SIGFPE,SIG_IGN);
#endif
#ifdef MUST_DISABLE_FPMASK
	fpsetmask(0);
#endif

SV*
rrd_error()
	CODE:
		if (! rrd_test_error()) XSRETURN_UNDEF;
                RETVAL = newSVpv(rrd_get_error(),0);
	OUTPUT:
		RETVAL

int
rrd_last(...)
      PROTOTYPE: @
      PREINIT:
      int i;
      char **argv;
      CODE:
              rrdcode(rrd_last);
      OUTPUT:
            RETVAL

int
rrd_first(...)
      PROTOTYPE: @
      PREINIT:
      int i;
      char **argv;
      CODE:
              rrdcode(rrd_first);
      OUTPUT:
            RETVAL

int
rrd_create(...)
	PROTOTYPE: @
	PREINIT:
        int i;
	char **argv;
	CODE:
		rrdcode(rrd_create);
	        RETVAL = 1;
        OUTPUT:
		RETVAL

int
rrd_update(...)
	PROTOTYPE: @
	PREINIT:
        int i;
	char **argv;
	CODE:
		rrdcode(rrd_update);
       	        RETVAL = 1;
	OUTPUT:
		RETVAL

int
rrd_tune(...)
	PROTOTYPE: @
	PREINIT:
        int i;
	char **argv;
	CODE:
		rrdcode(rrd_tune);
       	        RETVAL = 1;
	OUTPUT:
		RETVAL

#ifdef HAVE_RRD_GRAPH

SV *
rrd_graph(...)
	PROTOTYPE: @
	PREINIT:
	char **calcpr=NULL;
	int i,xsize,ysize;
	double ymin,ymax;
	char **argv;
	AV *retar;
	PPCODE:
		argv = (char **) malloc((items+1)*sizeof(char *));
		argv[0] = "dummy";
		for (i = 0; i < items; i++) {
		    STRLEN len;
		    char *handle = SvPV(ST(i),len);
		    /* actually copy the data to make sure possible modifications
		       on the argv data does not backfire into perl */
		    argv[i+1] = (char *) malloc((strlen(handle)+1)*sizeof(char));
		    strcpy(argv[i+1],handle);
 	        }
		rrd_clear_error();
		rrd_graph(items+1,argv,&calcpr,&xsize,&ysize,NULL,&ymin,&ymax);
		for (i=0; i < items; i++) {
		    free(argv[i+1]);
		}
		free(argv);

		if (rrd_test_error()) {
			if(calcpr) {
			   for(i=0;calcpr[i];i++)
				rrd_freemem(calcpr[i]);
                           rrd_freemem(calcpr);
                        }
			XSRETURN_UNDEF;
		}
		retar=newAV();
		if(calcpr){
			for(i=0;calcpr[i];i++){
				 av_push(retar,newSVpv(calcpr[i],0));
				 rrd_freemem(calcpr[i]);
			}
			rrd_freemem(calcpr);
		}
		EXTEND(sp,4);
		PUSHs(sv_2mortal(newRV_noinc((SV*)retar)));
		PUSHs(sv_2mortal(newSViv(xsize)));
		PUSHs(sv_2mortal(newSViv(ysize)));

#endif /* HAVE_RRD_GRAPH */

SV *
rrd_fetch(...)
	PROTOTYPE: @
	PREINIT:
		time_t        start,end;
		unsigned long step, ds_cnt,i,ii;
		rrd_value_t   *data,*datai;
		char **argv;
		char **ds_namv;
		AV *retar,*line,*names;
	PPCODE:
		argv = (char **) malloc((items+1)*sizeof(char *));
		argv[0] = "dummy";
		for (i = 0; i < items; i++) {
		    STRLEN len;
		    char *handle= SvPV(ST(i),len);
		    /* actually copy the data to make sure possible modifications
		       on the argv data does not backfire into perl */
		    argv[i+1] = (char *) malloc((strlen(handle)+1)*sizeof(char));
		    strcpy(argv[i+1],handle);
 	        }
		rrd_clear_error();
		rrd_fetch(items+1,argv,&start,&end,&step,&ds_cnt,&ds_namv,&data);
		for (i=0; i < items; i++) {
		    free(argv[i+1]);
		}
		free(argv);
		if (rrd_test_error()) XSRETURN_UNDEF;
                /* convert the ds_namv into perl format */
		names=newAV();
		for (ii = 0; ii < ds_cnt; ii++){
		    av_push(names,newSVpv(ds_namv[ii],0));
		    rrd_freemem(ds_namv[ii]);
		}
		rrd_freemem(ds_namv);
		/* convert the data array into perl format */
		datai=data;
		retar=newAV();
		for (i = start+step; i <= end; i += step){
			line = newAV();
			for (ii = 0; ii < ds_cnt; ii++){
 			  av_push(line,(isnan(*datai) ? newSV(0) : newSVnv(*datai)));
			  datai++;
			}
			av_push(retar,newRV_noinc((SV*)line));
		}
		rrd_freemem(data);
		EXTEND(sp,5);
		PUSHs(sv_2mortal(newSViv(start+step)));
		PUSHs(sv_2mortal(newSViv(step)));
		PUSHs(sv_2mortal(newRV_noinc((SV*)names)));
		PUSHs(sv_2mortal(newRV_noinc((SV*)retar)));

SV *
rrd_fetch_cb_register(cb)
    SV * cb
    CODE:
        if (rrd_fetch_cb_svptr == (SV*)NULL )
            rrd_fetch_cb_svptr = newSVsv(cb);
        else
            SvSetSV(rrd_fetch_cb_svptr,cb);
        rrd_fetch_cb_register(rrd_fetch_cb_wrapper);

SV *
rrd_times(start, end)
	  char *start
	  char *end
	PREINIT:
		rrd_time_value_t start_tv, end_tv;
		char    *parsetime_error = NULL;
		time_t	start_tmp, end_tmp;
	PPCODE:
		rrd_clear_error();
		if ((parsetime_error = rrd_parsetime(start, &start_tv))) {
			rrd_set_error("start time: %s", parsetime_error);
			XSRETURN_UNDEF;
		}
		if ((parsetime_error = rrd_parsetime(end, &end_tv))) {
			rrd_set_error("end time: %s", parsetime_error);
			XSRETURN_UNDEF;
		}
		if (rrd_proc_start_end(&start_tv, &end_tv, &start_tmp, &end_tmp) == -1) {
			XSRETURN_UNDEF;
		}
		EXTEND(sp,2);
		PUSHs(sv_2mortal(newSVuv(start_tmp)));
		PUSHs(sv_2mortal(newSVuv(end_tmp)));

int
rrd_xport(...)
	PROTOTYPE: @
	PREINIT:
                time_t start,end;
                int xsize;
		unsigned long step, col_cnt,i,ii;
		rrd_value_t *data,*ptr;
                char **argv,**legend_v;
		AV *retar,*line,*names;
	PPCODE:
		argv = (char **) malloc((items+1)*sizeof(char *));
		argv[0] = "dummy";
		for (i = 0; i < items; i++) {
		    STRLEN len;
		    char *handle = SvPV(ST(i),len);
		    /* actually copy the data to make sure possible modifications
		       on the argv data does not backfire into perl */
		    argv[i+1] = (char *) malloc((strlen(handle)+1)*sizeof(char));
		    strcpy(argv[i+1],handle);
 	        }
		rrd_clear_error();
		rrd_xport(items+1,argv,&xsize,&start,&end,&step,&col_cnt,&legend_v,&data);
		for (i=0; i < items; i++) {
		    free(argv[i+1]);
		}
		free(argv);
		if (rrd_test_error()) XSRETURN_UNDEF;

                /* convert the legend_v into perl format */
		names=newAV();
		for (ii = 0; ii < col_cnt; ii++){
		    av_push(names,newSVpv(legend_v[ii],0));
		    rrd_freemem(legend_v[ii]);
		}
		rrd_freemem(legend_v);

		/* convert the data array into perl format */
		ptr=data;
		retar=newAV();
		for (i = start+step; i <= end; i += step){
			line = newAV();
			for (ii = 0; ii < col_cnt; ii++){
 			  av_push(line,(isnan(*ptr) ? newSV(0) : newSVnv(*ptr)));
			  ptr++;
			}
			av_push(retar,newRV_noinc((SV*)line));
		}
		rrd_freemem(data);

		EXTEND(sp,7);
		PUSHs(sv_2mortal(newSViv(start+step)));
		PUSHs(sv_2mortal(newSViv(end)));
		PUSHs(sv_2mortal(newSViv(step)));
		PUSHs(sv_2mortal(newSViv(col_cnt)));
		PUSHs(sv_2mortal(newRV_noinc((SV*)names)));
		PUSHs(sv_2mortal(newRV_noinc((SV*)retar)));

SV*
rrd_info(...)
	PROTOTYPE: @
	PREINIT:
		rrd_info_t *data,*save;
                int i;
                char **argv;
		HV *hash;
	CODE:
		rrdinfocode(rrd_info);
    OUTPUT:
	   RETVAL

SV*
rrd_updatev(...)
	PROTOTYPE: @
	PREINIT:
		rrd_info_t *data,*save;
                int i;
                char **argv;
		HV *hash;
	CODE:
		rrdinfocode(rrd_update_v);
    OUTPUT:
	   RETVAL

#ifdef HAVE_RRD_GRAPH

SV*
rrd_graphv(...)
	PROTOTYPE: @
	PREINIT:
		rrd_info_t *data,*save;
                int i;
                char **argv;
		HV *hash;
	CODE:
		rrdinfocode(rrd_graph_v);
    OUTPUT:
	   RETVAL

#endif /* HAVE_RRD_GRAPH */

int
rrd_dump(...)
       PROTOTYPE: @
       PREINIT:
        int i;
       char **argv;
       CODE:
               rrdcode(rrd_dump);
                       RETVAL = 1;
       OUTPUT:
               RETVAL

int
rrd_restore(...)
       PROTOTYPE: @
       PREINIT:
        int i;
       char **argv;
       CODE:
               rrdcode(rrd_restore);
                       RETVAL = 1;
       OUTPUT:
               RETVAL

int
rrd_flushcached(...)
	PROTOTYPE: @
	PREINIT:
	int i;
	char **argv;
	CODE:
		rrdcode(rrd_flushcached);
	OUTPUT:
		RETVAL

SV*
rrd_list(...)
	PROTOTYPE: @
	PREINIT:
                char *data;
                char *ptr, *end;
                int i;
                char **argv;
		AV *list;
	PPCODE:
		argv = (char **) malloc((items+1)*sizeof(char *));
		argv[0] = "dummy";

		for (i = 0; i < items; i++) {
		    STRLEN len;
		    char *handle= SvPV(ST(i),len);
		    /* actually copy the data to make sure possible modifications
		       on the argv data does not backfire into perl */
		    argv[i+1] = (char *) malloc((strlen(handle)+1)*sizeof(char));
		    strcpy(argv[i+1],handle);
		}

                rrd_clear_error();

		data = rrd_list(items+1, argv);

                for (i=0; i < items; i++) {
		    free(argv[i+1]);
		}
		free(argv);

                if (rrd_test_error())
		    XSRETURN_UNDEF;

		list = newAV();

		ptr = data;
		end = strchr(ptr, '\n');

		while (end) {
		    *end = '\0';
		    av_push(list, newSVpv(ptr, 0));
		    ptr = end + 1;

		    if (strlen(ptr) == 0)
			    break;

		    end = strchr(ptr, '\n');
		}

		rrd_freemem(data);

		XPUSHs(sv_2mortal(newRV_noinc((SV*)list)));
