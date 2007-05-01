/*****************************************************************************
 * RRDtool 1.2.20  Copyright by Tobi Oetiker, 1997-2007
 *****************************************************************************
 * rrd_resize.c Alters size of an RRA
 *****************************************************************************
 * Initial version by Alex van den Bogaerdt
 *****************************************************************************/

#include "rrd_tool.h"

int
rrd_resize(int argc, char **argv)
{
    char		*infilename,outfilename[11]="resize.rrd";
    FILE		*infile,*outfile;
    rrd_t		rrdold,rrdnew;
    rrd_value_t		buffer;
    int			version;
    unsigned long	l,rra;
    long		modify;
    unsigned long	target_rra;
    int			grow=0,shrink=0;
    char		*endptr;

    infilename=argv[1];
    if (!strcmp(infilename,"resize.rrd")) {
        rrd_set_error("resize.rrd is a reserved name");
        return(-1);
    }
    if (argc!=5) {
        rrd_set_error("wrong number of parameters");
        return(-1);
    }

    target_rra=strtol(argv[2],&endptr,0);

    if (!strcmp(argv[3],"GROW")) grow=1;
    else if (!strcmp(argv[3],"SHRINK")) shrink=1;
    else {
        rrd_set_error("I can only GROW or SHRINK");
        return(-1);
    }

    modify=strtol(argv[4],&endptr,0);

    if ((modify<1)) {
        rrd_set_error("Please grow or shrink with at least 1 row");
        return(-1);
    }

    if (shrink) modify = -modify;


    if (rrd_open(infilename, &infile, &rrdold, RRD_READWRITE)==-1) {
        rrd_set_error("could not open RRD");
        return(-1);
    }
    if (LockRRD(infile) != 0) {
        rrd_set_error("could not lock original RRD");
        rrd_free(&rrdold);
        fclose(infile);
        return(-1);
    }

    if (target_rra >= rrdold.stat_head->rra_cnt) {
        rrd_set_error("no such RRA in this RRD");
        rrd_free(&rrdold);
        fclose(infile);
        return(-1);
    }

    if (modify < 0)
	if ((long)rrdold.rra_def[target_rra].row_cnt <= -modify) {
	    rrd_set_error("This RRA is not that big");
	    rrd_free(&rrdold);
	    fclose(infile);
	    return(-1);
	}

    rrdnew.stat_head = rrdold.stat_head;
    rrdnew.ds_def    = rrdold.ds_def;
    rrdnew.rra_def   = rrdold.rra_def;
    rrdnew.live_head = rrdold.live_head;
    rrdnew.pdp_prep  = rrdold.pdp_prep;
    rrdnew.cdp_prep  = rrdold.cdp_prep;
    rrdnew.rra_ptr   = rrdold.rra_ptr;

    version = atoi(rrdold.stat_head->version);
    switch (version) {
	case 3: break;
	case 1: rrdold.stat_head->version[3]='3';
		break;
	default: {
		rrd_set_error("Do not know how to handle RRD version %s",rrdold.stat_head->version);
		rrd_free(&rrdold);	
		fclose(infile);
		return(-1);
		}
    }

    if ((outfile=fopen(outfilename,"wb"))==NULL) {
        rrd_set_error("Can't create '%s'",outfilename);
        return(-1);
    }
    if (LockRRD(outfile) != 0) {
        rrd_set_error("could not lock new RRD");
        rrd_free(&rrdold);
        fclose(infile);
        fclose(outfile);
        return(-1);
    }
    fwrite(rrdnew.stat_head, sizeof(stat_head_t),1,outfile);
    fwrite(rrdnew.ds_def,sizeof(ds_def_t),rrdnew.stat_head->ds_cnt,outfile);
    fwrite(rrdnew.rra_def,sizeof(rra_def_t),rrdnew.stat_head->rra_cnt,outfile);
    fwrite(rrdnew.live_head,sizeof(live_head_t),1,outfile);
    fwrite(rrdnew.pdp_prep,sizeof(pdp_prep_t),rrdnew.stat_head->ds_cnt,outfile);
    fwrite(rrdnew.cdp_prep,sizeof(cdp_prep_t),rrdnew.stat_head->ds_cnt*rrdnew.stat_head->rra_cnt,outfile);
    fwrite(rrdnew.rra_ptr,sizeof(rra_ptr_t),rrdnew.stat_head->rra_cnt,outfile);

    /* Move the CDPs from the old to the new database.
    ** This can be made (much) faster but isn't worth the effort. Clarity
    ** is much more important.
    */

    /* Move data in unmodified RRAs
    */
    l=0;
    for (rra=0;rra<target_rra;rra++) {
        l+=rrdnew.stat_head->ds_cnt * rrdnew.rra_def[rra].row_cnt;
    }
    while (l>0) {
        fread(&buffer,sizeof(rrd_value_t),1,infile);
        fwrite(&buffer,sizeof(rrd_value_t),1,outfile);
        l--;
    }
    /* Move data in this RRA, either removing or adding some rows
    */
    if (modify>0) {
        /* Adding extra rows; insert unknown values just after the
        ** current row number.
        */
        l = rrdnew.stat_head->ds_cnt * (rrdnew.rra_ptr[target_rra].cur_row+1);
        while (l>0) {
            fread(&buffer,sizeof(rrd_value_t),1,infile);
            fwrite(&buffer,sizeof(rrd_value_t),1,outfile);
            l--;
        }
        buffer=DNAN;
        l=rrdnew.stat_head->ds_cnt * modify;
        while (l>0) {
            fwrite(&buffer,sizeof(rrd_value_t),1,outfile);
            l--;
        }
    } else {
        /* Removing rows. Normally this would be just after the cursor
        ** however this may also mean that we wrap to the beginning of
        ** the array.
        */
        signed long int remove_end=0;

        remove_end=(rrdnew.rra_ptr[target_rra].cur_row-modify)%rrdnew.rra_def[target_rra].row_cnt;
        if (remove_end <= (signed long int)rrdnew.rra_ptr[target_rra].cur_row) {
            while (remove_end >= 0) {
                fseek(infile,sizeof(rrd_value_t)*rrdnew.stat_head->ds_cnt,SEEK_CUR);
                rrdnew.rra_ptr[target_rra].cur_row--;
                rrdnew.rra_def[target_rra].row_cnt--;
                remove_end--;
                modify++;
            }
            remove_end=rrdnew.rra_def[target_rra].row_cnt-1;
        }
        for (l=0;l<=rrdnew.rra_ptr[target_rra].cur_row;l++) {
            unsigned int tmp;
            for (tmp=0;tmp<rrdnew.stat_head->ds_cnt;tmp++) {
                fread(&buffer,sizeof(rrd_value_t),1,infile);
                fwrite(&buffer,sizeof(rrd_value_t),1,outfile);
            }
        }
        while (modify<0) {
            fseek(infile,sizeof(rrd_value_t)*rrdnew.stat_head->ds_cnt,SEEK_CUR);
            rrdnew.rra_def[target_rra].row_cnt--;
            modify++;
        }
    }
    /* Move the rest of the CDPs
    */
    while (1) {
	fread(&buffer,sizeof(rrd_value_t),1,infile);
	if (feof(infile))
	    break;
        fwrite(&buffer,sizeof(rrd_value_t),1,outfile);
    }
    rrdnew.rra_def[target_rra].row_cnt += modify;
    fseek(outfile,sizeof(stat_head_t)+sizeof(ds_def_t)*rrdnew.stat_head->ds_cnt,SEEK_SET);
    fwrite(rrdnew.rra_def,sizeof(rra_def_t),rrdnew.stat_head->rra_cnt, outfile);
    fseek(outfile,sizeof(live_head_t),SEEK_CUR);
    fseek(outfile,sizeof(pdp_prep_t)*rrdnew.stat_head->ds_cnt,SEEK_CUR);
    fseek(outfile,sizeof(cdp_prep_t)*rrdnew.stat_head->ds_cnt*rrdnew.stat_head->rra_cnt,SEEK_CUR);
    fwrite(rrdnew.rra_ptr,sizeof(rra_ptr_t),rrdnew.stat_head->rra_cnt, outfile);
    
    fclose(outfile);
    rrd_free(&rrdold);
    fclose(infile);
    return(0);
}
