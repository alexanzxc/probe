#include <stdio.h>
#include <papi.h>

int main(){

int retval;
retval=PAPI_library_init(PAPI_VER_CURRENT);
if (retval!=PAPI_VER_CURRENT) {
    fprintf(stderr,"Error initializing PAPI! %s\n",
    PAPI_strerror(retval));
    return 0;
    }
int eventset=PAPI_NULL;

retval=PAPI_create_eventset(&eventset);
if (retval!=PAPI_OK) {
    fprintf(stderr,"Error creating eventset! %s\n",
    PAPI_strerror(retval));
    }


retval=PAPI_add_named_event(eventset,"PAPI_TOT_CYC");
if (retval!=PAPI_OK) {
	fprintf(stderr,"Error adding PAPI_TOT_CYC: %s\n",
	PAPI_strerror(retval));
    }

long long count;

PAPI_reset(eventset);
retval=PAPI_start(eventset);
if (retval!=PAPI_OK) {
	fprintf(stderr,"Error starting CUDA: %s\n",
		PAPI_strerror(retval));
}
//this is what we measure
    printf("Papi test!\n");

retval=PAPI_stop(eventset,&count);
if (retval!=PAPI_OK) {
   fprintf(stderr,"Error stopping:  %s\n",
                        PAPI_strerror(retval));
}
else {
      printf("Measured %lld cycles\n",count);
}
    return 0;
}