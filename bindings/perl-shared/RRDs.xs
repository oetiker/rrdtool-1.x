#ifdef __cplusplus
extern "C" {
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#ifdef __cplusplus
}
#endif

#include "../../src/rrd_tool.h"

/* perl 5.004 compatibility */
#if PERLPATCHLEVEL < 5 
#define PL_sv_undef sv_undef
#endif

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
		optind=0; opterr=0; \
		rrd_clear_error();\
		RETVAL=name(items+1,argv); \
		for (i=0; i < items; i++) {\
		    free(argv[i+1]);\
		} \
		free(argv);\
		\
		if (rrd_test_error()) XSRETURN_UNDEF;

/*
 * should not be needed if libc is linked (see ntmake.pl)
#ifdef WIN32
 #define free free
 #define malloc malloc
 #define realloc realloc
#endif
*/


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


void
rrd_graph(...)
	PROTOTYPE: @	
	PREINIT:
	char **calcpr;
	int i,xsize,ysize;
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
		optind=0; opterr=0; 
		rrd_clear_error();
		rrd_graph(items+1,argv,&calcpr,&xsize,&ysize); 
		for (i=0; i < items; i++) {
		    free(argv[i+1]);
		}
		free(argv);

		if (rrd_test_error()) {
			if(calcpr)
			   for(i=0;calcpr[i];i++)
				free(calcpr[i]);
			XSRETURN_UNDEF;
		}
		retar=newAV();
		if(calcpr){
			for(i=0;calcpr[i];i++){
				 av_push(retar,newSVpv(calcpr[i],0));
				 free(calcpr[i]);
			}
			free(calcpr);
		}
		EXTEND(sp,4);
		PUSHs(sv_2mortal(newRV_noinc((SV*)retar)));
		PUSHs(sv_2mortal(newSViv(xsize)));
		PUSHs(sv_2mortal(newSViv(ysize)));

void
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
		optind=0; opterr=0; 
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
		    free(ds_namv[ii]);
		}
		free(ds_namv);			
		/* convert the data array into perl format */
		datai=data;
		retar=newAV();
		for (i = start+step; i <= end; i += step){
			line = newAV();
			for (ii = 0; ii < ds_cnt; ii++){
 			  av_push(line,(isnan(*datai) ? &PL_sv_undef : newSVnv(*datai)));
			  datai++;
			}
			av_push(retar,newRV_noinc((SV*)line));
		}
		free(data);
		EXTEND(sp,5);
		PUSHs(sv_2mortal(newSViv(start+step)));
		PUSHs(sv_2mortal(newSViv(step)));
		PUSHs(sv_2mortal(newRV_noinc((SV*)names)));
		PUSHs(sv_2mortal(newRV_noinc((SV*)retar)));


int
rrd_xport(...)
	PROTOTYPE: @	
	PREINIT:
                time_t start,end;		
                int xsize;
		unsigned long step, col_cnt,row_cnt,i,ii;
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
		optind=0; opterr=0; 
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
		    free(legend_v[ii]);
		}
		free(legend_v);			

		/* convert the data array into perl format */
		ptr=data;
		retar=newAV();
		for (i = start+step; i <= end; i += step){
			line = newAV();
			for (ii = 0; ii < col_cnt; ii++){
 			  av_push(line,(isnan(*ptr) ? &PL_sv_undef : newSVnv(*ptr)));
			  ptr++;
			}
			av_push(retar,newRV_noinc((SV*)line));
		}
		free(data);

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
		info_t *data,*save;
                int i;
                char **argv;
		HV *hash;
	CODE:
		/* prepare argument list */
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
		optind=0; opterr=0; 
                rrd_clear_error();
                data=rrd_info(items+1, argv);
                for (i=0; i < items; i++) {
		    free(argv[i+1]);
		}
		free(argv);
                if (rrd_test_error()) XSRETURN_UNDEF;
                hash = newHV();
                while (data) {
		    save=data;
		/* the newSV will get copied by hv so we create it as a mortal to make sure
                   it does not keep hanging round after the fact */
#define hvs(VAL) hv_store_ent(hash, sv_2mortal(newSVpv(data->key,0)),VAL,0)		    
		    switch (data->type) {
		    case RD_I_VAL:
			if (isnan(data->value.u_val))
			    hvs(&PL_sv_undef);
			else
			    hvs(newSVnv(data->value.u_val));
			break;
		    case RD_I_CNT:
			hvs(newSViv(data->value.u_cnt));
			break;
		    case RD_I_STR:
			hvs(newSVpv(data->value.u_str,0));
			free(data->value.u_str);
			break;
		    }
#undefine hvs
		    free(data->key);
		    data = data->next;		    
		    free(save);
		}
                free(data);
                RETVAL = newRV_noinc((SV*)hash);
       OUTPUT:
		RETVAL


