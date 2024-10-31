#include <mpi.h>
#include <vector>
#include <algorithm>
#include <iostream>
#include <random>
#include <cstdlib>
#include <chrono>
#include <caliper/cali.h>
#include <caliper/cali-manager.h>
#include <adiak.hpp>
#include <sstream>

void initialize_data(std::vector<int>& data_vector, long long size, const std::string& data_type) {
    CALI_MARK_BEGIN("data_init_runtime");
    data_vector.resize(size);

    if (data_type == "Sorted") {
        std::iota(data_vector.begin(), data_vector.end(), 0);
    } else if (data_type == "Reverse") {
        std::iota(data_vector.rbegin(), data_vector.rend(), 0);
    } else if (data_type == "Random") {
        std::random_device random_device;
        std::mt19937 generator(random_device());
        std::uniform_int_distribution<> distribution(0, size - 1);
        for (int& val : data_vector) {
            val = distribution(generator);
        }
    } else if (data_type == "Perturbed") {
        std::iota(data_vector.begin(), data_vector.end(), 0);
        int perturb_count = size / 100;
        std::random_device random_device;
        std::mt19937 generator(random_device());
        std::uniform_int_distribution<> distribution(0, size - 1);
        for (int i = 0; i < perturb_count; ++i) {
            std::swap(data_vector[distribution(generator)], data_vector[distribution(generator)]);
        }
    }
    CALI_MARK_END("data_init_runtime");
}

void merge_vectors(std::vector<int>& vec, long long left, long long mid, long long right) {
    std::vector<int> temp_buffer(right - left + 1);
    long long left_idx = left, right_idx = mid + 1, temp_idx = 0;

    while (left_idx <= mid && right_idx <= right) {
        temp_buffer[temp_idx++] = (vec[left_idx] <= vec[right_idx]) ? vec[left_idx++] : vec[right_idx++];
    }
    while (left_idx <= mid) {
        temp_buffer[temp_idx++] = vec[left_idx++];
    }
    while (right_idx <= right) {
        temp_buffer[temp_idx++] = vec[right_idx++];
    }
    for (temp_idx = 0; temp_idx < temp_buffer.size(); temp_idx++){
        vec[left + temp_idx] = temp_buffer[temp_idx];
    }
}

void recursive_merge_sort(std::vector<int>& vec, long long start, long long end) {
    if (start < end) {
        long long middle = start + (end - start) / 2;
        recursive_merge_sort(vec, start, middle);
        recursive_merge_sort(vec, middle + 1, end);
        merge_vectors(vec, start, middle, end);
    }
}

void distributed_merge_sort(std::vector<int>& data_vector, int rank, int world_size) {
    size_t global_size = (rank == 0) ? data_vector.size() : 0;
    MPI_Bcast(&global_size, 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);
    size_t chunk_size = global_size / world_size;
    size_t extra = global_size % world_size;

    std::vector<int> segment_counts(world_size), displacements(world_size);
    size_t offset = 0;
    for (int i = 0; i < world_size; ++i) {
        segment_counts[i] = static_cast<int>(chunk_size + (i < extra ? 1 : 0));
        displacements[i] = static_cast<int>(offset);
        offset += segment_counts[i];
    }

    MPI_Bcast(segment_counts.data(), world_size, MPI_INT, 0, MPI_COMM_WORLD);
    size_t local_data_size = segment_counts[rank];
    std::vector<int> local_data(local_data_size);

    CALI_MARK_BEGIN("comm");
    CALI_MARK_BEGIN("comm_large");
    MPI_Scatterv(rank == 0 ? data_vector.data() : nullptr, segment_counts.data(), displacements.data(), MPI_INT, local_data.data(), local_data_size, MPI_INT, 0, MPI_COMM_WORLD);
    CALI_MARK_END("comm_large");
    CALI_MARK_END("comm");

    CALI_MARK_BEGIN("comp");
    CALI_MARK_BEGIN("comp_large");
    recursive_merge_sort(local_data, 0, local_data_size - 1);
    CALI_MARK_END("comp_large");
    CALI_MARK_END("comp");

    std::vector<int> all_sizes(world_size);
    MPI_Allgather(&local_data_size, 1, MPI_INT, all_sizes.data(), 1, MPI_INT, MPI_COMM_WORLD);

    for (int step = 1; step < world_size; step *= 2) {
        if (rank % (2 * step) == 0 && (rank + step < world_size)) {
            int neighbor_data_size = all_sizes[rank + step];
            std::vector<int> received_data;

            CALI_MARK_BEGIN("comm");
            CALI_MARK_BEGIN("comm_large");
            MPI_Status status;
            MPI_Probe(rank + step, 0, MPI_COMM_WORLD, &status);
            int actual_size;
            MPI_Get_count(&status, MPI_INT, &actual_size);

            received_data.resize(actual_size);
            MPI_Recv(received_data.data(), actual_size, MPI_INT, rank + step, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            CALI_MARK_END("comm_large");
            CALI_MARK_END("comm");
            CALI_MARK_BEGIN("comp");
            CALI_MARK_BEGIN("comp_large");
            std::vector<int> merged_data(local_data.size() + received_data.size());
            std::merge(local_data.begin(), local_data.end(), received_data.begin(), received_data.end(), merged_data.begin());
            local_data = std::move(merged_data);
            CALI_MARK_END("comp_large");
            CALI_MARK_END("comp");
        } else if (rank % (2 * step) != 0) {
            int target_rank = rank - step;
            if (target_rank >= 0) {
                CALI_MARK_BEGIN("comm");
                CALI_MARK_BEGIN("comm_large");
                MPI_Send(local_data.data(), local_data.size(), MPI_INT, target_rank, 0, MPI_COMM_WORLD);
                CALI_MARK_END("comm_large");
                CALI_MARK_END("comm");
                break;
            }
        }
    }

    if (rank == 0) data_vector = std::move(local_data);
}

bool verify_sorted(const std::vector<int>& vec) {
    CALI_MARK_BEGIN("correctness_check");
    bool sorted = std::is_sorted(vec.begin(), vec.end());
    CALI_MARK_END("correctness_check");
    
    return sorted;
}

int main(int argc, char** argv) {
    CALI_MARK_BEGIN("main");
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, nullptr);
    int rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (argc != 3) {
        if (rank == 0) {
            std::cerr << "Usage: " << argv[0] << " <array_size> <input_type>" << std::endl;
        }
        MPI_Finalize();
        return 1;
    }

    size_t data_size = std::atoi(argv[1]);
    std::string data_type = argv[2];

    adiak::init(NULL);
    adiak::launchdate();
    adiak::libraries();
    adiak::cmdline();
    adiak::clustername();
    adiak::value("algorithm", "merge");
    adiak::value("programming_model", "mpi");
    adiak::value("data_type", "int");
    adiak::value("size_of_data_type", sizeof(int));
    adiak::value("input_size", data_size);
    adiak::value("input_type", data_type);
    adiak::value("num_procs", world_size);
    adiak::value("scalability", "strong");
    adiak::value("group_num", 19);
    adiak::value("implementation_source", "handwritten");

    std::vector<int> data;
    if (rank == 0) {
        initialize_data(data, data_size, data_type);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    distributed_merge_sort(data, rank, world_size);

    if (rank == 0) {
        bool is_sorted = verify_sorted(data);
    }

    MPI_Finalize();
    CALI_MARK_END("main");

    return 0;
}
