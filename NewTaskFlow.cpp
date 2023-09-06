#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <chrono>
#include <vector>
#include <mutex>
#include "../include/taskflow/taskflow.hpp"
#include "../include/taskflow/algorithm/for_each.hpp"
#include "../include/taskflow/algorithm/sort.hpp"
#include "../include/taskflow/algorithm/reduce.hpp"
#include "../include/taskflow/core/task.hpp"
#pragma warning(disable:4146)

struct MergeSortParams {
    double** distances;
    int datasetSize;
    int low;
    int high;
};

class TaskflowKnn {
private:
    int neighbours_number;

public:
    TaskflowKnn(int k) : neighbours_number(k) {}

    int predict_class(double* dataset[], const double* target, int dataset_size, int feature_size) {
        double* distances[3];
        int zeros_count = 0;
        int ones_count = 0;

        // Allocate memory for distances and index order
        distances[0] = new double[dataset_size];
        distances[1] = new double[dataset_size];
        distances[2] = new double[dataset_size];

        get_knn(dataset, target, distances, dataset_size, feature_size);

        // Parallelized sorting using Taskflow
        tf::Executor executor;
        tf::Taskflow taskflow;
        

        taskflow.emplace([&]() {
           quick_sort(distances, 0, dataset_size - 1);
           });
        executor.run(taskflow).wait();
        
        

        //std::mutex distances_mutex;
        //std::mutex index_order_mutex;

        /*int* index_order = new int[dataset_size];
        for (int i = 0; i < dataset_size; ++i) {
            index_order[i] = i;
        }*/

        //// Create a thread-safe comparison function
        //auto compare_function = [&](int i, int j) {
        //    std::lock_guard<std::mutex> lock(distances_mutex);
        //    double diff = distances[0][i] - distances[0][j];
        //    return (diff < 0) ? -1 : (diff > 0) ? 1 : 0;
        //};

        //// Parallelized sorting using Taskflow
        //tf::Executor executor;
        //tf::Taskflow taskflow;

        //taskflow.emplace([&, index_order]() {
        //    std::lock_guard<std::mutex> lock(index_order_mutex);
        //    taskflow.sort(index_order, index_order + static_cast<int>(neighbours_number), compare_function);
        //    });
        //executor.run(taskflow).wait();

       

        /*taskflow.emplace([&]() {
            taskflow.sort(index_order, index_order + static_cast<int>(neighbours_number), [&](int i, int j) {
                return static_cast<int>(distances[0][i] - distances[0][j]);
                });
            });
        executor.run(taskflow).wait();*/

        for (int i = 0; i < 20; i++) {
            std::cout << distances[0][i] << "," << distances[1][i] << "," << distances[2][i] << std::endl;
        }

        // Count label occurrences in the K nearest neighbors
        for (int i = 0; i < neighbours_number; i++) {
            if (distances[1][i] == 0) {
                zeros_count += 1;
                std::cout << "0: " << distances[0][i] << "," << distances[2][i] << std::endl;
            }
            else if (distances[1][i] == 1) {
                ones_count += 1;
                std::cout << "1: " << distances[0][i] << "," << distances[2][i] << std::endl;
            }
        }

        int prediction = (zeros_count > ones_count) ? 0 : 1;

        // Clean up memory
        delete[] distances[0];
        delete[] distances[1];
        delete[] distances[2];
        //delete[] index_order;

        return prediction;
    }

private:
    static int partition(double** distances, int low, int high) {
        double pivot = distances[0][high];
        int i = low - 1;
        for (int j = low; j < high; j++) {
            if (distances[0][j] <= pivot) {
                i++;
                swap(distances, i, j);
            }
        }
        swap(distances, i + 1, high);
        printf("A");
        return i + 1;
    }

    static void swap(double** distances, int i, int j) {
        std::swap(distances[0][i], distances[0][j]);
        std::swap(distances[1][i], distances[1][j]);
        std::swap(distances[2][i], distances[2][j]);
        printf("B");    
    }

    static void quick_sort(double** distances, int low, int high) {
        tf::Executor executor;
        tf::Taskflow taskflow;
        std::stack<std::pair<int, int>> stack;
        stack.push(std::make_pair(low, high));


        while (!stack.empty()) {
            std::pair<int, int> range = stack.top();
            stack.pop();

            int low = range.first;
            int high = range.second;

            printf("C");

            if (low < high) {
                int pivotIndex = partition(distances, low, high);

                taskflow.emplace([&](tf::Subflow& sf) {
                    sf.emplace([&]() {
                        stack.push(std::make_pair(low, pivotIndex - 1));
                        });
                    sf.emplace([&]() {
                        stack.push(std::make_pair(pivotIndex + 1, high));
                        });
                    });
                executor.run(taskflow);
                //if (low < high) {
                //    int pivotIndex = partition(distances, low, high);
                //    //quick_sort(distances, low, pivotIndex - 1);
                //    //quick_sort(distances, pivotIndex + 1, high);

                //        taskflow.emplace([&](tf::Subflow& sf) {
                //            sf.emplace([&]() {
                //                quick_sort(distances, low, pivotIndex - 1);
                //                });
                //            sf.emplace([&]() {
                //                quick_sort(distances, pivotIndex + 1, high);
                //                });
                //            });
                //        executor.run(taskflow).wait();
                //}
            }
        }
        
    }

    double euclidean_distance(const double* x, const double* y, int feature_size) {
        double l2 = 0.0;
        for (int i = 1; i < feature_size; i++) {
            l2 += std::pow((x[i] - y[i]), 2);
        }
        return std::sqrt(l2);
    }

    void get_knn(double* x[], const double* y, double* distances[3], int dataset_size, int feature_size) {
        tf::Executor executor;
        tf::Taskflow taskflow;

        //int num_task = 4;
        //int step_size = dataset_size / 13420;
        
        // Parallelized loop using Taskflow's for_each_index algorithm
        taskflow.for_each_index(0, dataset_size, 1, [&, y, x, feature_size](int i) {
            if (x[i] == y) return;
            distances[0][i] = this->euclidean_distance(y, x[i], feature_size);
            distances[1][i] = x[i][0]; // Store outcome label
            distances[2][i] = i; // Store index
            });

        executor.run(taskflow).wait();

        std::cout << "Number of euclidean run: " << dataset_size << std::endl;
    }

};

std::vector<double> parseLine(const std::string& line) {
    std::vector<double> row;
    std::istringstream iss(line);
    std::string value;

    while (std::getline(iss, value, ',')) {
        try {
            double num = std::stod(value);
            row.push_back(num);
        }
        catch (const std::invalid_argument&) {
            std::cerr << "Invalid data in CSV: " << value << std::endl;
        }
    }

    return row;
}

int main() {
    std::string filename = "diabetes_binary.csv";

    const int dataset_size = 53681;
    const int feature_size = 22;

    double** dataset = new double* [dataset_size];
    //double target[feature_size] = { 0.0, 0.0, 0.0, 1.0, 24.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0, 1.0, 0.0, 1.0, 3.0, 0.0, 0.0, 0.0, 2.0, 5.0, 3.0 };
    double target[feature_size] = { 1.0, 1.0, 1.0, 1.0, 30.0, 1.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 5.0, 30.0, 30.0, 1.0, 0.0, 9.0, 5.0, 1.0 };

    for (int i = 0; i < dataset_size; i++) {
        dataset[i] = new double[feature_size];
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return 1;
    }

    std::string header;
    std::getline(file, header);

    std::string line;
    int index = 0;
    while (std::getline(file, line) && index < dataset_size) {
        std::vector<double> row = parseLine(line);
        for (int j = 0; j < feature_size; j++) {
            dataset[index][j] = row[j];
        }
        index++;
    }

    std::cout << "Number of records: " << index << std::endl;

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    TaskflowKnn knn(3);

    int prediction = knn.predict_class(dataset, target, dataset_size, feature_size);
    std::cout << "Prediction: " << prediction << std::endl;

    if (prediction == 0) {
        std::cout << "Predicted class: Negative" << std::endl;
    }
    else if (prediction == 1) {
        std::cout << "Predicted class: Prediabetes or Diabetes" << std::endl;
    }
    else {
        std::cout << "Prediction could not be made." << std::endl;
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count() << "[�s]" << std::endl;

    for (int i = 0; i < dataset_size; i++) {
        delete[] dataset[i];
    }
    delete[] dataset;

    return 0;
}
