# CSCE 435 Group project

## 0. Group number: 19

## 1. Project details:

### 1a. Group members:
1. Tran Lam
2. Andrew Oh
3. Jerry Tran
4. Nick Janocik

### 1b. Communication method:
We will be using Discord to communicate with one another.

## 2. Project topic: _Parallel Sorts_

### 2a. Brief project description (what algorithms will you be comparing and on what architectures)

- Bitonic Sort:
- Sample Sort:
- Merge Sort: This sorting algorithm follows the "divide-and-conquer" approach when sorting an array of elements. The array is split up into smaller sub-arrays, those sub-arrays are then sorted, and finally, the subarrays are merged together to complete the sort. We will be implementing a parallel version of the merge sort using MPI which will help distribute the computation across multiple processors. Then, we will compare the performeance in terms of execution time and resource utilization across different parallelization configurations.
- Radix Sort: This is a non-comparative sorting algorithm that sorts integers/strings by processing individual digits/characters. It will be implemented in a parallel manner and will be tested on multi-core processors. Grace is what we are going to use which allows us to measure performance metric such as execution time and resource utilization across different configurations.

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes
- Bitonic Sort:

- Sample Sort:

- Merge Sort:
````
def ParallelMergeSort(array A, integer n):
    MPI_Init()
    rank = MPI_Comm_rank(MPI_COMM_WORLD)
    num_procs = MPI_Comm_size(MPI_COMM_WORLD)

    local_n = n / num_procs
    array local_A[local_n]
    MPI_Scatter(A, local_n, MPI_INT, local_A, local_n, MPI_INT, 0, MPI_COMM_WORLD)

    MergeSort(local_A, 0, local_n - 1)

    size = 1
    while size < num_procs:
        if (rank MOD (2 * size)) == 0:
            if rank + size < num_procs:
                recv_array = new array of size(local_n)
                MPI_Recv(recv_array, local_n, MPI_INT, rank + size, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE)
                local_A = Merge(local_A, recv_array)
        else:
            partner = rank - size
            MPI_Send(local_A, local_n, MPI_INT, partner, 0, MPI_COMM_WORLD)
            BREAK
        size = size * 2

    if rank == 0:
        MPI_Gather(local_A, local_n, MPI_INT, A, local_n, MPI_INT, 0, MPI_COMM_WORLD)

    MPI_Finalize()

def Merge(array left, array right):
    int left_len = length(left)
    int right_len = length(right)
    array result[left_len + right_len]
    int i, j, k = 0

    while i < left_len AND j < right_len:
        if left[i] <= right[j]:
            result[k] = left[i]
            i = i + 1
        else:
            result[k] = right[j]
            j = j + 1
        k = k + 1

    while i < left_len:
        result[k] = left[i]
        i = i + 1
        k = k + 1

    while j < right_len:
        result[k] = right[j]
        j = j + 1
        k = k + 1

    return result

def MergeSort(array A, integer left, integer right):
    if left < right:
        mid = (left + right) / 2
        MergeSort(A, left, mid)
        MergeSort(A, mid + 1, right)
        A = Merge(A[left...mid], A[mid+1...right])
````


- Radix Sort:
````
FUNCTION RadixSort(array A, integer n)
    // Step 1: Find the maximum number to determine the number of digits
    max = FindMax(A, n)

    // Step 2: Do counting sort for every digit
    FOR digit = 1 TO max_digits(max) DO
        CountingSortByDigit(A, n, digit)
    END FOR
END FUNCTION

FUNCTION FindMax(array A, integer n)
    max = A[0]
    FOR i = 1 TO n - 1 DO
        IF A[i] > max THEN
            max = A[i]
        END IF
    END FOR
    RETURN max
END FUNCTION

FUNCTION CountingSortByDigit(array A, integer n, integer digit)
    // Step 3: Create output array and count array
    array output[n]
    array count[10] INITIALIZED TO 0

    // Step 4: Count occurrences of each digit
    FOR i = 0 TO n - 1 DO
        index = (A[i] / digit) MOD 10
        count[index] = count[index] + 1
    END FOR

    // Step 5: Change count[i] so that it contains the actual position of this digit in output[]
    FOR i = 1 TO 9 DO
        count[i] = count[i] + count[i - 1]
    END FOR

    // Step 6: Build the output array
    FOR i = n - 1 DOWN TO 0 DO
        index = (A[i] / digit) MOD 10
        output[count[index] - 1] = A[i]
        count[index] = count[index] - 1
    END FOR

    // Step 7: Copy the output array to A[]
    FOR i = 0 TO n - 1 DO
        A[i] = output[i]
    END FOR
END FUNCTION
````


### 2c. Evaluation plan - what and how will you measure and compare
- Input sizes, Input types
- Strong scaling (same problem size, increase number of processors/nodes)
- Weak scaling (increase problem size, increase number of processors)
