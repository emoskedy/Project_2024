#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <caliper/cali.h> 
#include <caliper/cali-manager.h>
#include <adiak.hpp>

#define RANDOM 0
#define SORTED 1
#define REVERSE_SORTED 2
#define NOISE 3

const char* main_cali = "main";
const char* data_init_runtime = "data_init_runtime";
const char* correctness_check = "correctness_check";
const char* comm = "comm";
const char* comm_small = "comm_small";
const char* comm_large = "comm_large";
const char* comp = "comp";
const char* comp_small = "comp_small";
const char* comp_large = "comp_large";

int* GenerateArray(int size, int inputType, int processorNumber, int totalProcesses) {
    int* arr = new int[size];

    // Seed the random number generator for better randomness
    srand(static_cast<unsigned int>(rand()) + processorNumber); 

    switch (inputType) {
        case RANDOM:
            // Generate random integers between 0 and 9999
            for (int i = 0; i < size; i++) 
            {
                arr[i] = rand() % 10000;
            }
            break;

        case SORTED:
            // Generate sorted integers with an offset for processor ID
            for (int i = 0; i < size; i++) 
            {
                arr[i] = i + (size * processorNumber); // Ensures distinct sorted arrays per processor
            }
            break;

        case REVERSE_SORTED:
            // Generate reverse sorted integers with an offset for processor ID
            for (int i = 0; i < size; i++) 
            {
                arr[i] = (size - i - 1) + (size * processorNumber); // Distinct reverse sorted arrays
            }
            break;

        case NOISE:
            // Create a sorted array first
            for (int i = 0; i < size; i++) {
                arr[i] = i + (size * processorNumber); 
            }

            // Randomly update 1% of the elements
            for (int i = 0; i < size / 100; ++i) 
            { 
                int index = rand() % size;
                arr[index] = rand() % 10000; // Random value
            }
            break;

        default:
            // Invalid input type; handle error
            printf("Invalid input type specified.\n");
            delete[] arr; // Clean up before returning
            return nullptr;
    }

    return arr;
}

void PrintArray(int* arr, int size, int processorNumber) {
    printf("Processor number %d, array:", processorNumber);
    for (int i = 0; i < size; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
}

bool SortedVerification(int* arr, int size) { // Check if an array was sorted correctly
    int max = 0;
    for (int i = 0; i < size; i++) {
        if (arr[i] < max) {
            return false;
        }
        max = arr[i];
    }
    return true;
}

int main(int argc, char *argv[]) 
{
    int sizeOfArray; // This is an input to the code
    int inputType;
    std::string inputString;

    if (argc == 3) {
        sizeOfArray = atoi(argv[1]);
        inputType = atoi(argv[2]);
        if (inputType < 0 || inputType > 3) {
            printf("\n input type [0-3]");
            return 0;
        }
    } else {
        printf("\n arguments [sizeOfArray] [input type]");
        return 0;
    }

    if (inputType == RANDOM)
        inputString = "random";
    if (inputType == SORTED)
        inputString = "sorted";
    if (inputType == REVERSE_SORTED)
        inputString = "reversed";
    if (inputType == NOISE)
        inputString = "noise";

    int taskid;
    int numTasks;
    int destPos = 0;

    MPI_Status status;

    cali::ConfigManager mgr;
    mgr.start();

    CALI_MARK_BEGIN(main_cali);
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
    MPI_Comm_size(MPI_COMM_WORLD, &numTasks);

    CALI_MARK_BEGIN(data_init_runtime);
    int* arr = GenerateArray(sizeOfArray, inputType, taskid, numTasks);
    CALI_MARK_END(data_init_runtime);

    int rank = 0;
    while (rank < numTasks) {
        if (rank == taskid) {
            if (rank == 0) {
                printf("Initializing Arrays\n");
            }
            PrintArray(arr, sizeOfArray, taskid);
            fflush(stdout);
        }
        rank++;
        MPI_Barrier(MPI_COMM_WORLD);
    }

    for (int i = 0; i < 32; ++i) 
    { 
        CALI_MARK_BEGIN(comp);
        CALI_MARK_BEGIN(comp_large);

        // Performing a local counting sort
        int countingArray[2] = {0, 0};
        for (int j = 0; j < sizeOfArray; ++j) {
            countingArray[(arr[j] >> i) & 1]++;
        }

        countingArray[1] += countingArray[0];
        int numLocalZeroes = countingArray[0];
        int* newArr = new int[sizeOfArray];

        for (int j = sizeOfArray - 1; j >= 0; --j) {
            newArr[countingArray[(arr[j] >> i) & 1] - 1] = arr[j];
            countingArray[(arr[j] >> i) & 1]--;
        }

        delete[] arr;
        arr = newArr;

        CALI_MARK_END(comp_large);
        CALI_MARK_END(comp);

        // Communication for gathering zero and one counts
        CALI_MARK_BEGIN(comm);
        CALI_MARK_BEGIN(comm_small);
        
        int numLocalOnes = sizeOfArray - numLocalZeroes;
        int numTotalZeroes = 0;
        MPI_Reduce(&numLocalZeroes, &numTotalZeroes, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
        
        // Get previous numbers of 0s and 1s to determine placement
        int numPrevZeroes = 0;
        int numPrevOnes = 0;
        
        for (int j = 0; j < numTasks; ++j) {
            if (j == taskid) {
                for (int k = taskid + 1; k < numTasks; ++k) {
                    MPI_Send(&numLocalZeroes, 1, MPI_INT, k, 1, MPI_COMM_WORLD);
                    MPI_Send(&numLocalOnes, 1, MPI_INT, k, 1, MPI_COMM_WORLD);
                }
            } else {
                if (j < taskid) {
                    int numPrevZeroesBuf, numPrevOnesBuf;
                    MPI_Recv(&numPrevZeroesBuf, 1, MPI_INT, j, 1, MPI_COMM_WORLD, &status);
                    MPI_Recv(&numPrevOnesBuf, 1, MPI_INT, j, 1, MPI_COMM_WORLD, &status);
                    numPrevZeroes += numPrevZeroesBuf;
                    numPrevOnes += numPrevOnesBuf;
                }
            }
        }

        CALI_MARK_END(comm_small);
        CALI_MARK_BEGIN(comm_large);
        
        int* newSortedArr = new int[sizeOfArray];
        int* destProcArray = new int[sizeOfArray];
        int* destPosArray = new int[sizeOfArray];
        bool* present = new bool[sizeOfArray]();

        for (int j = 0; j < sizeOfArray; ++j) {
            int position = (arr[j] >> i) & 1 ? (j + numTotalZeroes + numPrevOnes - numLocalZeroes) : (j + numPrevZeroes);
            destProcArray[j] = position / sizeOfArray;
            destPosArray[j] = position % sizeOfArray;

            if (destProcArray[j] == taskid) {
                newSortedArr[destPosArray[j]] = arr[j];
                present[destPosArray[j]] = true;
            }
        }

        for (int j = 0; j < sizeOfArray; ++j) {
            for (int k = 0; k < numTasks; ++k) {
                if (k == destProcArray[j] && k != taskid) {
                    MPI_Send(&destPosArray[j], 1, MPI_INT, k, 1, MPI_COMM_WORLD);
                    MPI_Send(&arr[j], 1, MPI_INT, k, 2, MPI_COMM_WORLD);
                }
                if (k == taskid && !present[j]) {
                    MPI_Recv(&destPos, 1, MPI_INT, MPI_ANY_SOURCE, 1, MPI_COMM_WORLD, &status);
                    MPI_Recv(&newSortedArr[destPos], 1, MPI_INT, MPI_ANY_SOURCE, 2, MPI_COMM_WORLD, &status);
                }
            }
        }

        delete[] arr;
        arr = newSortedArr;
        CALI_MARK_END(comm_large);
        CALI_MARK_END(comm);
        
    }
    
    // Make sure all arrays finish sorting 
    MPI_Barrier(MPI_COMM_WORLD);

    CALI_MARK_BEGIN(correctness_check);
    bool arraySorted = true;
    if (!SortedVerification(arr, sizeOfArray)) {
        printf("Array not sorted, processor %d\n", taskid);
        arraySorted = false;
    }
    for (int i = 0; i < numTasks; ++i) {
        if (i != 0 && i == taskid) {
            int maxPrevArray;
            MPI_Recv(&maxPrevArray, 1, MPI_INT, taskid - 1, 1, MPI_COMM_WORLD, &status);
            if (maxPrevArray > arr[sizeOfArray - 1]) {
                printf("Array not sorted, between processor %d and %d\n", taskid - 1, taskid);
                arraySorted = false;
            }
        }
        if (i != numTasks - 1 && i == taskid) {
            MPI_Send(&arr[sizeOfArray - 1], 1, MPI_INT, taskid + 1, 1, MPI_COMM_WORLD);
        }
    }
    if (arraySorted) {
        printf("Array Sorted!\n");
    }
    CALI_MARK_END(correctness_check);
    CALI_MARK_END(main_cali);

    // Print finished arrays
    rank = 0;
    while (rank < numTasks) {
        if (rank == taskid) {
            if (rank == 0) {
                printf("Finished Arrays\n");
            }
            PrintArray(arr, sizeOfArray, taskid);
            fflush(stdout);
        }
        rank++;
        MPI_Barrier(MPI_COMM_WORLD);
    }

    adiak::init(NULL);
    adiak::launchdate();    // Launch date of the job
    adiak::libraries();     // Libraries used
    adiak::cmdline();       // Command line used to launch the job
    adiak::clustername();   // Name of the cluster
    adiak::value("algorithm", "radix"); // The name of the algorithm you are using (e.g., "merge", "bitonic")
    adiak::value("programming_model", "mpi"); // e.g., "mpi"
    adiak::value("data_type", "int"); // The datatype of input elements (e.g., double, int, float)
    adiak::value("size_of_data_type", sizeof(int)); // sizeof(datatype) of input elements in bytes (e.g., 1, 2, 4)
    adiak::value("input_size", sizeOfArray); // The number of elements in input dataset (1000)
    adiak::value("input_type", inputString); // For sorting, this would be choices: ("Sorted", "ReverseSorted", "Random", "Noise")
    adiak::value("num_procs", numTasks); // The number of processors (MPI ranks)
    adiak::value("scalability", "strong"); // The scalability of your algorithm. choices: ("strong", "weak")
    adiak::value("group_num", 19); // The number of your group (integer, e.g., 1, 10)
    adiak::value("implementation_source", "ai"); // Where you got the source code of your algorithm. choices: ("online", "ai", "handwritten").

    mgr.stop();
    mgr.flush();

    MPI_Finalize();
    return 0;
}
