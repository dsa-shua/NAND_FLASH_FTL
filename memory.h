#include <stdlib.h>
#include <stdio.h>

#define DEBUG 0
#define NUM_DATA_B 4    
#define NUM_SLOG_B 2    // Non zero for all
#define NUM_RLOG_B 0    // 0 for BAST
#define BLOCK_SIZE 4    // number of cells
#define NUM_BLOCKS (NUM_DATA_B + NUM_SLOG_B + NUM_RLOG_B)


enum BLOCK_TYPE {DATA_BLOCK, SEQ_LOG, RAN_LOG};
struct MemCell{
    unsigned int valid; // 0 for free, 1 for correct, 2 for invalid
    unsigned int data;
};

struct Block{
    enum BLOCK_TYPE block_type;
    struct MemCell cells[BLOCK_SIZE];
    int assigned_block; // BAST: for log blocks (-1 for Free blocks), -2 for data blks (no updated cells), -3 for data blks active log
};

struct Block* NAND_BASE = 0;

void init(void)
{   
    printf("Initializing Memory\n");
    NAND_BASE = (struct Block*)malloc(sizeof(struct Block) * NUM_BLOCKS);

    for(unsigned int i = 0; i < NUM_DATA_B; i++){
        NAND_BASE[i].block_type = DATA_BLOCK;
        NAND_BASE[i].assigned_block = -2; // -2 for data blocks
        printf("[%d] D\n",i);
        for(unsigned j = 0; j < BLOCK_SIZE; j++){
            NAND_BASE[i].cells[j].valid = 0;
            NAND_BASE[i].cells[j].data = 0;
            printf("    [%d]: V: %d | D: %5d\n",j,NAND_BASE[i].cells[j].valid, NAND_BASE[i].cells[j].data);
        }

    }

    // BAST: Normal Log Block, FAST: Sequential
    for(unsigned int i = NUM_DATA_B; i < NUM_DATA_B + NUM_SLOG_B; i++){
        NAND_BASE[i].block_type = SEQ_LOG;
        NAND_BASE[i].assigned_block = -1; // Free block 
        printf("[%d] S | Assigned: %d\n",i, NAND_BASE[i].assigned_block);
        for(unsigned j = 0; j < BLOCK_SIZE; j++){
            NAND_BASE[i].cells[j].valid = 0;
            NAND_BASE[i].cells[j].data = 0;
            printf("    [%d]: V: %d | D: %5d\n",j,NAND_BASE[i].cells[j].valid, NAND_BASE[i].cells[j].data);
        }
        
    }

    for(unsigned int i = (NUM_DATA_B+NUM_SLOG_B); i < NUM_BLOCKS; i++){
        NAND_BASE[i].block_type = RAN_LOG;
        printf("[%d] R\n",i);
        for(unsigned j = 0; j < BLOCK_SIZE; j++){
            NAND_BASE[i].cells[j].valid = 0;
            NAND_BASE[i].cells[j].data = 0;
            printf("    [%d]: V: %d | D: %5d\n",j,NAND_BASE[i].cells[j].valid, NAND_BASE[i].cells[j].data);
        }
        
    }

}

void dealloc(void){
    printf("Exit\n");
    free(NAND_BASE);
    NAND_BASE = 0;
}

void printNAND(void){
    printf("\n\n******************** PRINTING NAND *************************\n");

    for(unsigned blk = 0; blk < NUM_BLOCKS; blk++){
        printf("BLK NUM: %d | TYPE: %d | Assigned: %d\n", blk, NAND_BASE[blk].block_type, NAND_BASE[blk].assigned_block);
        for(unsigned cell = 0; cell < BLOCK_SIZE; cell++){
            printf("    [%d]: { V: %d | D: %5d }\n", cell, NAND_BASE[blk].cells[cell].valid,NAND_BASE[blk].cells[cell].data);
        }
    }

    printf("\n****************************************************************\n\n");
}

void write(unsigned int lsn, unsigned int write_data)
{
    unsigned int target_blk = lsn / NUM_DATA_B;
    unsigned int target_off = lsn % NUM_DATA_B;

    printf("\n\n Write Func: { lsn: %d }\n", lsn);
    printf("NAND_BASE:      %15p\n", NAND_BASE);
    printf("Target BLK:     %15d\n", target_blk);
    printf("Target Offset:  %15d\n", target_off);
    printf("Writing Data:   %15d\n", write_data);


    // Write when only free
    if (NAND_BASE[target_blk].cells[target_off].valid == 0){
        if(NAND_BASE[target_blk].assigned_block == -2){
            printf("Valid block to write\n");
            NAND_BASE[target_blk].cells[target_off].valid = 1;
            NAND_BASE[target_blk].cells[target_off].data = write_data;
        }
        else if (NAND_BASE[target_blk].assigned_block == -3){
            printf("Should write on the log block instead\n");

            for(unsigned blk = NUM_DATA_B; blk < (NUM_DATA_B + NUM_SLOG_B); blk++){
                if (NAND_BASE[blk].assigned_block == target_blk){
                    printf("Log block for this block still active, write here\n");
                    NAND_BASE[blk].cells[target_off].data = write_data;
                    NAND_BASE[blk].cells[target_off].valid = 1;
                    NAND_BASE[target_blk].cells[target_off].valid = 2; // make it invalid becuase correct data in log blks
                }
            }
        }

    } 
    // When the cell is currently occupied -> make it invalid, write to LOG BLOCK
    else if (NAND_BASE[target_blk].cells[target_off].valid == 1) {

        unsigned int found_free = 0;
        int found_free_blk_num = -1;
        unsigned int victim_block;
        unsigned int victim_blk_assigned;

        // Check if there are free log blocks
        for(unsigned int start_log = NUM_DATA_B; start_log < (NUM_DATA_B + NUM_SLOG_B); start_log++){
            if (NAND_BASE[start_log].assigned_block == -1){
                NAND_BASE[start_log].assigned_block = target_blk; // set this block
                found_free++; // set found free_block flag.
                found_free_blk_num = (int)start_log;
                break;
            } else continue;
        }


        // if we found a free log block
        if(found_free){
            NAND_BASE[(unsigned int)found_free_blk_num].cells[target_off].data = write_data;
            NAND_BASE[(unsigned int)found_free_blk_num].cells[target_off].valid = 1;

            // Finally, invalidate the block
            NAND_BASE[target_blk].cells[target_off].valid = 2;
            NAND_BASE[target_blk].assigned_block = -3; // make sure new writes are on the log blks
        }
        
        // make merge operation
        else if (!found_free){
            // find a victim block
            unsigned int found_victim = 0;
            for(unsigned int start_log = NUM_DATA_B; start_log < (NUM_DATA_B + NUM_SLOG_B); start_log++){
                if(NAND_BASE[start_log].assigned_block >= 0){
                    victim_block = start_log;
                    victim_blk_assigned = NAND_BASE[start_log].assigned_block;
                    found_victim++;
                    break;
                }
            }

            // perform merge operation
            if(found_victim){
                printf("\nVictim block: %d\n", victim_block);
                printf("victim_blk_assigned: %d\n", victim_blk_assigned);

                for(unsigned int cell = 0; cell < BLOCK_SIZE; cell++){
                    printf("Current cell: %d\n", cell);
                    if(NAND_BASE[victim_block].cells[cell].valid == 1){
                        NAND_BASE[victim_blk_assigned].cells[cell].data = NAND_BASE[victim_block].cells[cell].data;
                        NAND_BASE[victim_blk_assigned].cells[cell].valid = 1;
                        NAND_BASE[victim_blk_assigned].assigned_block = -2; // now a valid data block

                        // Clear on log part
                        // NAND_BASE[victim_block].assigned_block = - 1;   // block is now read for new writes
                        NAND_BASE[victim_block].cells[cell].data = 0;  // clear data
                        NAND_BASE[victim_block].cells[cell].valid = 0; // make free cell
                    }
                }


                // Perform the new write on the log block
                NAND_BASE[victim_block].assigned_block = target_blk;
                NAND_BASE[victim_block].cells[target_off].data = write_data;
                NAND_BASE[victim_block].cells[target_off].valid = 1;
            }
            else {
                printf("Panic: < cannot find victim block >\n");
            }

            



        }

    }

    // When the most recent data is on the log block -> perform merge operation
    else if (NAND_BASE[target_blk].cells[target_off].valid == 2) {
        printf("Perform merge operation\n");

        unsigned int corr_log_blk;
        // find the corresponding log block
        for(unsigned int blk = NUM_DATA_B; blk < (NUM_DATA_B + NUM_SLOG_B); blk++){
            if (NAND_BASE[blk].assigned_block == target_blk){
                corr_log_blk = blk; // plug
                break;
            }
        }

        for(unsigned int cell = 0; cell < BLOCK_SIZE; cell++){
            printf("cell: %d\n", cell);

            // updated data must be on the cell
            if(NAND_BASE[target_blk].cells[cell].valid == 2){

                if(target_off == cell){
                    // printf("==============[ write_Data: %d ]========\n",write_data);
                    NAND_BASE[target_blk].cells[cell].data = write_data;
                }else {
                    NAND_BASE[target_blk].cells[cell].data = NAND_BASE[corr_log_blk].cells[cell].data;
                }
                
                NAND_BASE[target_blk].cells[cell].valid = 1;
                

                NAND_BASE[corr_log_blk].cells[cell].valid = 0; // make them free
                NAND_BASE[corr_log_blk].cells[cell].data = 0;
            }
            else if(NAND_BASE[target_blk].cells[cell].valid == 1){
                // do nothing
            }
        }

        // Clean up the process
        NAND_BASE[target_blk].assigned_block = -2;
        NAND_BASE[corr_log_blk].assigned_block = -1;
    }

    printNAND();

}