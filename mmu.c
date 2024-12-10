#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "list.h"
#include "util.h"

void TOUPPER(char * arr){
  
    for(int i=0;i<strlen(arr);i++){
        arr[i] = toupper(arr[i]);
    }
}

void get_input(char *args[], int input[][2], int *n, int *size, int *policy) 
{
  	FILE *input_file = fopen(args[1], "r");
	  if (!input_file) {
		    fprintf(stderr, "Error: Invalid filepath\n");
		    fflush(stdout);
		    exit(0);
	  }

    parse_file(input_file, input, n, size);
  
    fclose(input_file);
  
    TOUPPER(args[2]);
  
    if((strcmp(args[2],"-F") == 0) || (strcmp(args[2],"-FIFO") == 0))
        *policy = 1;
    else if((strcmp(args[2],"-B") == 0) || (strcmp(args[2],"-BESTFIT") == 0))
        *policy = 2;
    else if((strcmp(args[2],"-W") == 0) || (strcmp(args[2],"-WORSTFIT") == 0))
        *policy = 3;
    else {
       printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
       exit(1);
    }
        
}

void allocate_memory(list_t *freelist, list_t *alloclist, int pid, int blocksize, int policy) {
    node_t *current = freelist->head;
    node_t *prev = NULL;
    node_t *best_fit = NULL;
    node_t *worst_fit = NULL;
    node_t *best_fit_prev = NULL;
    node_t *worst_fit_prev = NULL;

    while (current != NULL) {
        block_t *blk = current->blk;
        int blk_size = blk->end - blk->start + 1;

        if (blk_size >= blocksize) {
            if (policy == 1) { // First Fit
                break;
            } else if (policy == 2) { // Best Fit
                if (best_fit == NULL || blk_size < (best_fit->blk->end - best_fit->blk->start + 1)) {
                    best_fit = current;
                    best_fit_prev = prev;
                }
            } else if (policy == 3) { // Worst Fit
                if (worst_fit == NULL || blk_size > (worst_fit->blk->end - worst_fit->blk->start + 1)) {
                    worst_fit = current;
                    worst_fit_prev = prev;
                }
            }
        }

        prev = current;
        current = current->next;
    }

    // Select block based on policy
    node_t *selected_node = NULL;
    node_t *selected_prev = NULL;

    if (policy == 1) {
        selected_node = current;
        selected_prev = prev;
    } else if (policy == 2) {
        selected_node = best_fit;
        selected_prev = best_fit_prev;
    } else if (policy == 3) {
        selected_node = worst_fit;
        selected_prev = worst_fit_prev;
    }

    if (selected_node == NULL) {
        printf("Error: Not Enough Memory\n");
        return;
    }

    block_t *selected_blk = selected_node->blk;
    int original_end = selected_blk->end;

    // Remove the selected block from the free list
    if (selected_prev == NULL) {
        freelist->head = selected_node->next;
    } else {
        selected_prev->next = selected_node->next;
    }

    // Allocate the memory
    selected_blk->pid = pid;
    selected_blk->end = selected_blk->start + blocksize - 1;
    list_add_ascending_by_address(alloclist, selected_blk);

    // Handle fragmentation
    if (selected_blk->end < original_end) {
        block_t *fragment = malloc(sizeof(block_t));
        fragment->pid = 0;
        fragment->start = selected_blk->end + 1;
        fragment->end = original_end;

        if (policy == 1) {
            list_add_to_back(freelist, fragment); // FIFO
        } else if (policy == 2) {
            list_add_ascending_by_blocksize(freelist, fragment); // Best Fit
        } else if (policy == 3) {
            list_add_descending_by_blocksize(freelist, fragment); // Worst Fit
        }
    }

    free(selected_node); // Free the node structure
}

void deallocate_memory(list_t *alloclist, list_t *freelist, int pid, int policy) {
    node_t *current = alloclist->head;
    node_t *prev = NULL;

    while (current != NULL) {
        block_t *blk = current->blk;

        if (blk->pid == pid) {
            // Remove the block from the allocated list
            if (prev == NULL) {
                alloclist->head = current->next;
            } else {
                prev->next = current->next;
            }

            blk->pid = 0; // Reset PID to 0

            // Add the block back to the free list based on policy
            if (policy == 1) {
                list_add_to_back(freelist, blk); // FIFO
            } else if (policy == 2) {
                list_add_ascending_by_blocksize(freelist, blk); // Best Fit
            } else if (policy == 3) {
                list_add_descending_by_blocksize(freelist, blk); // Worst Fit
            }

            free(current); // Free the node structure
            return;
        }

        prev = current;
        current = current->next;
    }

    printf("Error: Can't locate Memory Used by PID: %d\n", pid);
}

list_t* coalese_memory(list_t * list){
  list_t *temp_list = list_alloc();
  block_t *blk;
  
  while((blk = list_remove_from_front(list)) != NULL) {  // sort the list in ascending order by address
        list_add_ascending_by_address(temp_list, blk);
  }
  
  // try to combine physically adjacent blocks
  
  list_coalese_nodes(temp_list);
        
  return temp_list;
}

void print_list(list_t * list, char * message){
    node_t *current = list->head;
    block_t *blk;
    int i = 0;
  
    printf("%s:\n", message);
  
    while(current != NULL){
        blk = current->blk;
        printf("Block %d:\t START: %d\t END: %d", i, blk->start, blk->end);
      
        if(blk->pid != 0)
            printf("\t PID: %d\n", blk->pid);
        else  
            printf("\n");
      
        current = current->next;
        i += 1;
    }
}

/* DO NOT MODIFY */
int main(int argc, char *argv[]) 
{
   int PARTITION_SIZE, inputdata[200][2], N = 0, Memory_Mgt_Policy;
  
   list_t *FREE_LIST = list_alloc();   // list that holds all free blocks (PID is always zero)
   list_t *ALLOC_LIST = list_alloc();  // list that holds all allocated blocks
   int i;
  
   if(argc != 3) {
       printf("usage: ./mmu <input file> -{F | B | W }  \n(F=FIFO | B=BESTFIT | W-WORSTFIT)\n");
       exit(1);
   }
  
   get_input(argv, inputdata, &N, &PARTITION_SIZE, &Memory_Mgt_Policy);
  
   // Allocated the initial partition of size PARTITION_SIZE
   
   block_t * partition = malloc(sizeof(block_t));   // create the partition meta data
   partition->start = 0;
   partition->end = PARTITION_SIZE + partition->start - 1;
                                   
   list_add_to_front(FREE_LIST, partition);          // add partition to free list
                                   
   for(i = 0; i < N; i++) // loop through all the input data and simulate a memory management policy
   {
       printf("************************\n");
       if(inputdata[i][0] != -99999 && inputdata[i][0] > 0) {
             printf("ALLOCATE: %d FROM PID: %d\n", inputdata[i][1], inputdata[i][0]);
             allocate_memory(FREE_LIST, ALLOC_LIST, inputdata[i][0], inputdata[i][1], Memory_Mgt_Policy);
       }
       else if (inputdata[i][0] != -99999 && inputdata[i][0] < 0) {
             printf("DEALLOCATE MEM: PID %d\n", abs(inputdata[i][0]));
             deallocate_memory(ALLOC_LIST, FREE_LIST, abs(inputdata[i][0]), Memory_Mgt_Policy);
       }
       else {
             printf("COALESCE/COMPACT\n");
             FREE_LIST = coalese_memory(FREE_LIST);
       }   
     
       printf("************************\n");
       print_list(FREE_LIST, "Free Memory");
       print_list(ALLOC_LIST,"\nAllocated Memory");
       printf("\n\n");
   }
  
   list_free(FREE_LIST);
   list_free(ALLOC_LIST);
  
   return 0;
}
