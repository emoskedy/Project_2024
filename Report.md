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
- Bitonic Sort: This sorting algorithm sorts an array of elements by creating a bitonic sequence then sort the sequence. A bitonic sequence is a sequence that is formed by two halves, one monotonically increases and one monotonically decreases. Then the sequence use a network of comparators called Bitonic merge network and swaps elements to ensure correct order. We will be using Grace to measure performance metric such as execution time and resource utiliztion across different sizes of arrays and the number of processors.
- Sample Sort: The input array is divided among multiple processes, each of which sorts its local chunk independently. The sorted chunks are then iteratively merged in parallel, reducing the number of processes involved at each step. The merging continues until only the root process has the final sorted array, which is gathered from all processes.
- Merge Sort: This sorting algorithm follows the "divide-and-conquer" approach when sorting an array of elements. The array is split up into smaller sub-arrays, those sub-arrays are then sorted, and finally, the subarrays are merged together to complete the sort. We will be implementing a parallel version of the merge sort using MPI which will help distribute the computation across multiple processors. Then, we will compare the performance in terms of execution time and resource utilization across different parallelization configurations.
- Radix Sort: This is a non-comparative sorting algorithm that sorts integers/strings by processing individual digits/characters. It will be implemented in a parallel manner and will be tested on multi-core processors. Grace is what we are going to use which allows us to measure performance metric such as execution time and resource utilization across different configurations.

### 2b. Pseudocode for each parallel algorithm
- For MPI programs, include MPI calls you will use to coordinate between processes
- Bitonic Sort:
````
BitonicSort(array A, low l, count cnt, direction d, rank r, size s)
    if count > 1
        k = count / 2
        BitonicSort(A, l, k, 1, r, s)
        BitonicSort(A, l + k, k, 0, r, s)
        BitonicMerge(A, l, cnt, d, r, s)

BitonicMerge(array A, low l, count cnt, direction d, rank r, size s)
    if count > 1
        k = count / 2
        for i = low to low + k - 1
            if (d == 1 and A[i] > A[i + k] or d == 0 and A[i] < A[i + k])
                swap(A[i], A[i + k])
        partner = rank ^ k
        if partner < size
            MPI_Send(A, cnt, MPI_INT, partner, 0, MPI_COMM_WORLD)
            MPI_Recv(A, cnt, MPI_INT, partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE)
        BitonicMerge(A, l, k, d, r, s)
        BitonicMerge(A, l + k, k, d, r, s)

Init_Main(argc, argv[])
    array_size = atoi(argv[1])
    num_procs = atoi(argv[2])

    MPI_Init(&argc, &argv)
    rank = MPI_Comm_rank(MPI_COMM_WORLD)
    size = MPI_Comm_rank(MPI_COMM_WORLD)
    arr = allocate_array_part(rank, size, array_size)

    BitonicSort(arr, 0, array_size, 1, rank, size)

    MPI_Finalize()    
````
- Sample Sort:
````
SampleSort(argc, argv)
    MPI_Init(&argc, &argv)
    rank = MPI_Comm_rank(MPI_COMM_WORLD)
    num_procs = MPI_Comm_size(MPI_COMM_WORLD)

    if rank == 0
        array A = generateArray(argv[1]) 
        n = length(arr)
    else
        array A = NULL

    local_arr = distributeArray(A, n, num_procs, r)
    local_sample = selectSample(local_arr, num_procs)
    all_samples = MPI_Gather(local_sample, num_procs - 1, MPI_INT, root=0)
    if rank == 0
        all_samples_sorted = sort(all_samples)
        split = selectGlobalSplitters(all_samples_sorted, num_procs)

    MPI_Bcast(split, num_procs - 1, MPI_INT, root=0)
    partitions = partition(local_arr, split, num_procs)
    all_partitions = MPI_Alltoall(partitions, num_procs)
    local_sorted_arr = mergePartitions(all_partitions)
    sorted_arr = MPI_Gather(local_sorted_arr, local_size, MPI_INT, root=0)

    MPI_Finalize()
    return sorted_arr

distributeArray(array A, n, num_procs, rank r)
    local_size = n / num_procs
    local_arr = MPI_Scatter(A, local_size, MPI_INT, root=0)
    return local_arr

selectSample(local_arr LA, num_procs)
    sample_size = num_procs - 1
    sample = randomSample(LA, sample_size)
    sample_sorted = sort(sample)
    return sample_sorted

partition(local_arr LA, split, num_procs)
    partitions = createEmptyPartitions(num_procs)
    for each element in LA
        find correct partition based on splitters
        add element to corresponding partition

    return partitions

mergePartitions(partitions)
    merged_arr = []
    merged_arr = kWayMerge(partitions)

    return merged_arr

kWayMerge(partitions)
    Merges all partitions into a single sorted array
    return merged_array
````
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
FUNCTION ParallelRadixSort(array A, integer n, integer rank, integer size)
    // Step 1: Find the maximum number in the entire array
    max = MPI_Reduce(FindMax(A), MPI_MAX, root=0)

    // Step 2: Split the data among processes
    local_n = n / size
    array local_A[local_n]
    MPI_Scatter(A, local_A, local_n, root=0)

    // Step 3: Perform local Radix Sort
    RadixSort(local_A, local_n)

    // Step 4: Gather sorted subarrays
    array sorted_A[n]
    MPI_Gather(local_A, sorted_A, local_n, root=0)

    IF rank == 0 THEN
        // Step 5: Final Merge (if needed)
        FinalMerge(sorted_A)
    END IF
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

FUNCTION RadixSort(array A, integer n)
    // Find the maximum number to determine the number of digits
    max = FindMax(A, n)

    // Do counting sort for every digit
    FOR digit = 1 TO max_digits(max) DO
        CountingSortByDigit(A, n, digit)
    END FOR
END FUNCTION

FUNCTION CountingSortByDigit(array A, integer n, integer digit)
    // Step 1: Create output array and count array
    array output[n]
    array count[10] INITIALIZED TO 0

    // Step 2: Count occurrences of each digit
    FOR i = 0 TO n - 1 DO
        index = (A[i] / digit) MOD 10
        count[index] = count[index] + 1
    END FOR

    // Step 3: Change count[i] so that it contains the actual position of this digit in output[]
    FOR i = 1 TO 9 DO
        count[i] = count[i] + count[i - 1]
    END FOR

    // Step 4: Build the output array
    FOR i = n - 1 DOWN TO 0 DO
        index = (A[i] / digit) MOD 10
        output[count[index] - 1] = A[i]
        count[index] = count[index] - 1
    END FOR

    // Step 5: Copy the output array to A[]
    FOR i = 0 TO n - 1 DO
        A[i] = output[i]
    END FOR
END FUNCTION
````


### 2c. Evaluation plan - what and how will you measure and compare
We plan to evaluate the sorting algorithms by changing the input sizes of the arrays and comparing the performance between each instance. We will also test different input types such as random arrays, sorted arrays, and reversed arrays.

To evaluate strong scaling, we will be testing the same sizes and types of arrays but increasing the number of processors or nodes and comparing execution time and efficiency.

For weak scaling, we will be increasing both the problem size and number of processors proportionally and measuring the execution time and efficiency.

Lastly, we will compare the overall performance across the different sorting algorithms (Bitonic, Sample, Merge, and Radix Sort) based on scalability, execution time, and resource utilization. We will be using Grace to measure each and compare the computations.
