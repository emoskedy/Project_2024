#include <mpi.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <random>
#include <cstdlib>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>

// merge two sorted vectors
void mergeArrays(const std::vector<int>& left, const std::vector<int>& right, std::vector<int>& merged) {
    CALI_CXX_MARK_FUNCTION;
    merged.clear();
    merged.reserve(left.size() + right.size());
    size_t i = 0, j = 0;
    while (i < left.size() && j < right.size()) {
        if (left[i] <= right[j]) {
            merged.push_back(left[i++]);
        } else {
            merged.push_back(right[j++]);
        }
    }
    
    merged.insert(merged.end(), left.begin() + i, left.end());
    merged.insert(merged.end(), right.begin() + j, right.end());
}

void recursiveMergeSort(std::vector<int>& arr, int start, int end) {
    CALI_CXX_MARK_FUNCTION;
    if (start < end) {
        int mid = start + (end - start) / 2;

        std::vector<int> left(arr.begin() + start, arr.begin() + mid + 1);
        std::vector<int> right(arr.begin() + mid + 1, arr.begin() + end + 1);
        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_small");
        recursiveMergeSort(left, 0, left.size() - 1);
        recursiveMergeSort(right, 0, right.size() - 1);
        CALI_MARK_END("comp_small");
        CALI_MARK_END("comp");

        CALI_MARK_BEGIN("comp");
        CALI_MARK_BEGIN("comp_large");
        std::vector<int> sorted(end - start + 1);
        mergeArrays(left, right, sorted);
        CALI_MARK_END("comp_large");
        CALI_MARK_END("comp");

        std::copy(sorted.begin(), sorted.end(), arr.begin() + start);
    }
}

void executeParallelMergeSort(std::vector<int>& arr, int array_size, int rank, int num_procs) {
    CALI_CXX_MARK_FUNCTION;

    int chunk_size = array_size / num_procs;
    int remainder = array_size % num_procs;

    std::vector<int> local_chunk((rank == num_procs - 1) ? (chunk_size + remainder) : chunk_size);

    std::vector<int> send_counts(num_procs);
    std::vector<int> displacements(num_procs);

    for (int i = 0; i < num_procs; ++i) {
        send_counts[i] = (i == num_procs - 1) ? (chunk_size + remainder) : chunk_size;
        displacements[i] = i * chunk_size;
    }

    MPI_Scatterv(rank == 0 ? arr.data() : nullptr, send_counts.data(), displacements.data(), MPI_INT, 
                 local_chunk.data(), local_chunk.size(), MPI_INT, 0, MPI_COMM_WORLD);

    recursiveMergeSort(local_chunk, 0, local_chunk.size() - 1);

    MPI_Gatherv(local_chunk.data(), local_chunk.size(), MPI_INT, arr.data(), send_counts.data(),
                displacements.data(), MPI_INT, 0, MPI_COMM_WORLD);
}

void initializeData(std::vector<int>& arr, int size, const std::string& pattern) {
    CALI_CXX_MARK_FUNCTION;
    arr.resize(size);
    CALI_MARK_BEGIN("data_init_runtime");
    if (pattern == "Sorted") {
        for (int i = 0; i < size; ++i) {
            arr[i] = i;
        }
    } else if (pattern == "ReverseSorted") {
        for (int i = 0; i < size; ++i) {
            arr[i] = size - i - 1;
        }
    } else if (pattern == "Random") {
        std::generate(arr.begin(), arr.end(), []() { return rand() % 10000; });
    } else if (pattern == "1_perc_perturbed") {
        for (int i = 0; i < size; ++i) {
            arr[i] = i;
        }
        for (int i = 0; i < size / 100; ++i) {
            int idx1 = rand() % size;
            int idx2 = rand() % size;
            std::swap(arr[idx1], arr[idx2]);
        }
    }
    CALI_MARK_END("data_init_runtime");
}

bool validateSorted(const std::vector<int>& arr) {
    CALI_CXX_MARK_FUNCTION;
    CALI_MARK_BEGIN("correctness_check");
    bool sorted = std::is_sorted(arr.begin(), arr.end());
    CALI_MARK_END("correctness_check");
    
    return sorted;
}

int main(int argc, char** argv) {
    CALI_CXX_MARK_FUNCTION;
    cali::ConfigManager mgr;
    mgr.add("runtime-report");
    mgr.start();
    MPI_Init(&argc, &argv);
    int rank, num_procs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &num_procs);

    int array_size = 0;
    std::string input_type = "Random"; 

    if (rank == 0) {
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " <array_size> <input_type>" << std::endl;
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        array_size = std::atoi(argv[1]);
        input_type = argv[2];
    }

    MPI_Bcast(&array_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
    int input_type_length = input_type.length();
    MPI_Bcast(&input_type_length, 1, MPI_INT, 0, MPI_COMM_WORLD);

    char* input_type_buffer = new char[input_type_length + 1];
    if (rank == 0) {
        std::strcpy(input_type_buffer, input_type.c_str());
    }
    MPI_Bcast(input_type_buffer, input_type_length + 1, MPI_CHAR, 0, MPI_COMM_WORLD);
    input_type = std::string(input_type_buffer);
    delete[] input_type_buffer;

    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", "parallel_merge_sort");
    adiak::value("programming_model", "MPI");
    adiak::value("data_type", "int");
    adiak::value("size_of_data_type", sizeof(int));
    adiak::value("input_size", array_size);
    adiak::value("input_type", input_type);
    adiak::value("num_procs", num_procs);
    adiak::value("scalability", "strong");
    adiak::value("group_num", 19);
    adiak::value("implementation_source", "handwritten");

    std::vector<int> data(array_size);
    if (rank == 0) {
        initializeData(data, array_size, input_type);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    executeParallelMergeSort(data, array_size, rank, num_procs);

    if (rank == 0) {
        if (validateSorted(data)) {
            std::cout << "Array is sorted correctly." << std::endl;
        } else {
            std::cout << "Array is NOT sorted correctly." << std::endl;
        }
    }

    mgr.stop();
    mgr.flush();

    MPI_Finalize();
    return 0;
}
