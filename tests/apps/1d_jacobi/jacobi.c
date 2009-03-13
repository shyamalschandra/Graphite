#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#include "carbon_user.h"
#include "capi.h"
#include "sync_api.h"

#define DEBUG 1
// #define FULL_DEBUG 1
// #define USE_INT 1

#ifdef FULL_DEBUG
#define DEBUG 1
#endif

#ifdef USE_INT
typedef SInt32 DType;
#else
typedef float DType;
#endif

#define NUM_ITERS 50
#define SWAP(a,b,t)  (((t) = (a)), ((a) = (b)), ((b) = (t)))

// Global Variables
DType* g_old_array;
DType* g_new_array;
SInt32 g_num_cores;
SInt32 g_size;

carbon_barrier_t jacobi_barrier;

#ifdef DEBUG
pthread_mutex_t print_lock;
#endif

// Function Declarations
void* threadMain(void*);
void printArray(SInt32);

SInt32 wait_some()
{
   SInt32 j = 0;
   for (UInt32 i = 0; i < 2000; i++)
   {
      j += i;
   }
   return j;
}

SInt32 main(SInt32 argc, char *argv[])
{
   CarbonStartSim();
   if (argc != 3) {
      fprintf(stderr, "[Usage]: ./jacobi <Number_of_Cores> <Size_Multiplier>\n");
                fprintf(stderr, "Size_of_Array = Number_of_Cores * Size_Multiplier\n"); 
      exit(-1);
   }

   g_num_cores = atoi(argv[1]);
   int size_multiplier = atoi (argv[2]);
   if (size_multiplier < 1) {
      fprintf(stderr, "Size_Multiplier should be >= 1\n");
      fprintf(stderr, "Note: Size_of_Array = Number_of_Cores * Size_Multiplier\n");
      exit(-1);
   }
   
   g_size = g_num_cores * size_multiplier;

   fprintf (stderr, "Number of Cores = %d\n", g_num_cores);
   fprintf (stderr, "Size of the array = %d\n", g_size);

   g_old_array = (DType *) malloc((g_size+2) * sizeof(DType));
   g_new_array = (DType *) malloc((g_size+2) * sizeof(DType));

   for (SInt32 i = 0; i < g_size+2; i++)
   {
      g_old_array[i] = 0;
      g_new_array[i] = 0;
   }
   g_old_array[0] = 32768;
   g_new_array[0] = 32768;

#ifdef DEBUG
   pthread_mutex_init(&print_lock, NULL);
#endif

   pthread_t threads[g_num_cores];
   pthread_attr_t attr;

   pthread_attr_init(&attr);
   pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

#ifdef DEBUG
   pthread_mutex_lock(&print_lock);
   fprintf(stderr, "Creating Threads\n");
   pthread_mutex_unlock(&print_lock);
#endif

   for (SInt32 i = 0; i < g_num_cores; i++)
   {
#ifdef FULL_DEBUG
      pthread_mutex_lock(&print_lock);
      fprintf(stderr, "Creating Thread: %d\n", i);
      pthread_mutex_unlock(&print_lock);
#endif

      SInt32 ret = pthread_create(&threads[i], &attr, threadMain, (void*) i);
      if (ret != 0) {
         // fprintf(stderr, "ERROR spawning thread %d! Error code: %s\n",
         //       i, strerror(ret));         
         fprintf(stderr, "ERROR spawning thread %d!\n", i);         
         exit(-1);
      }
   }

   for (SInt32 i = 0; i < g_num_cores; i++)
   {
      pthread_join(threads[i], NULL);
   }

   printArray(NUM_ITERS);

   CarbonStopSim();
   return (0);
}

void* threadMain(void *threadid)
{
   SInt32 tid = (SInt32) threadid;

   CAPI_Initialize(tid);

   if (tid == 0)
   {
#ifdef DEBUG
      pthread_mutex_lock(&print_lock);
      fprintf(stderr, "Starting threadMain - Thread 0\n");
      pthread_mutex_unlock(&print_lock);
#endif

#ifdef FULL_DEBUG
      pthread_mutex_lock(&print_lock);
      printArray(-1);
      pthread_mutex_unlock(&print_lock);
#endif

      barrierInit(&jacobi_barrier, g_num_cores);
   }
   else
   {
      sleep(1);
   }

   if (tid != 0)
   {
      wait_some();
   }

#ifdef FULL_DEBUG
   pthread_mutex_lock(&print_lock);
   fprintf(stderr, "Thread [ %d ]: Waiting for barrier\n", tid);
   pthread_mutex_unlock(&print_lock);
#endif

   barrierWait(&jacobi_barrier);

#ifdef DEBUG
   pthread_mutex_lock(&print_lock);
   fprintf(stderr, "Thread [ %d ]: Finished barrier\n", tid);
   pthread_mutex_unlock(&print_lock);
#endif

   SInt32 start_index = ((g_size/g_num_cores) * tid) + 1;
   SInt32 end_index = ((g_size/g_num_cores) * (tid + 1)) + 1;

#ifdef FULL_DEBUG
   pthread_mutex_lock(&print_lock);
   fprintf(stderr, "Thread [ %d ]: Start Index = %d\n", tid, start_index);
   fprintf(stderr, "Thread [ %d ]: End Index = %d\n", tid,  end_index);
   pthread_mutex_unlock(&print_lock);
#endif

   for (SInt32 k = 0; k < NUM_ITERS; k++)
   {
      for (SInt32 i = start_index; i < end_index; i++)
      {
         g_new_array[i] = (g_old_array[i-1] + g_old_array[i+1]) / 2;
      }

      barrierWait(&jacobi_barrier);

      if (tid == 0)
      {
         DType* temp;
         SWAP(g_old_array, g_new_array, temp);

#ifdef FULL_DEBUG
         pthread_mutex_lock(&print_lock);
         printArray(k);
         pthread_mutex_unlock(&print_lock);
#endif
      }

      barrierWait(&jacobi_barrier);

   }

   return 0;

}

void printArray(SInt32 iter)
{

   fprintf(stderr, "Contents of Array after iteration: %d\n", iter);
   for (SInt32 i = 0; i < g_size+2; i++)
   {
#ifdef USE_INT
      fprintf(stderr, "%d, ", g_old_array[i]);
#else
      fprintf(stderr, "%f, ", g_old_array[i]);
      UInt32 float_rep = *((UInt32*) &g_old_array[i]);
      // fprintf(stderr, "0x%x } , ", float_rep);
      // float_rep = float_rep & 0x7fc00000;
      if ( (float_rep & 0x7f800000) == 0x7f800000) {
         fprintf(stderr, "\n\nEncountered a NaN : 0x%x !!!\n\n", float_rep);
         assert (false);
         exit(-1);
      }
#endif
   }
   fprintf(stderr, "\n\n");

}
