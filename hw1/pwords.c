/*
 * Word count application with one thread per input file.
 *
 * You may modify this file in any way you like, and are expected to modify it.
 * Your solution must read each input file from a separate thread. We encourage
 * you to make as few changes as necessary.
 */

/*
 * Copyright © 2021 University of California, Berkeley
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <pthread.h>

#include "word_count.h"
#include "word_helpers.h"

/* Struct for passing args. */
struct arguments {
    word_count_list_t *word_counts_ptr;
    char *file_name;
};

/* Helper function used to add multi-threading functionality. */
void* threads_helper(void* args) {
    struct arguments *args_obj = (struct arguments *) args;

    FILE *file_ptr = fopen(args_obj->file_name, "r");
    if (file_ptr == NULL) {
        pthread_exit(NULL);
    }
    count_words(args_obj->word_counts_ptr, file_ptr);

    fclose(file_ptr);
    pthread_exit(NULL);
}

/*
 * main - handle command line, spawning one thread per file.
 */
int main(int argc, char* argv[]) {
  /* Create the empty data structure. */
  word_count_list_t word_counts;
  init_words(&word_counts);

  if (argc <= 1) {
    /* Process stdin in a single thread. */
    count_words(&word_counts, stdin);
  } else {
    int num_threads = argc - 1;
    pthread_t *threads_arr = malloc(sizeof(pthread_t) * num_threads);
    if (threads_arr == NULL) {
        return -1;
    }

    struct arguments *args = (struct arguments *) malloc(sizeof(struct arguments));
    args->word_counts_ptr = &word_counts;
    pthread_mutex_init(&(word_counts.lock), NULL);

    long tid;
    for (tid = 0; tid < num_threads; ++tid) {
        args->file_name = argv[tid + 1];
        int return_val = pthread_create(&threads_arr[tid], NULL, threads_helper, (void *) args);
        if (return_val) {
            printf("ERROR; return code from pthread_create() is %d\n", return_val);
            exit(-1);
        }
    }

  for (tid = 0; tid < num_threads; ++tid) {
      int ret_val = pthread_join(threads_arr[tid], NULL);
      if (ret_val) {
          return -1;
      }
  }

  free(args);
  free(threads_arr);
  }

  /* Output final result of all threads' work. */
  wordcount_sort(&word_counts, less_count);
  fprint_words(&word_counts, stdout);
  return 0;
}
