#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <random>
#include <chrono>
#include <atomic>
#include <iomanip>

class DungeonManager {
private:
    std::mutex mtx;
    std::condition_variable cv;
    
    int tank_queue;
    int healer_queue;
    int dps_queue;
    
    int dungeon_count;
    std::vector<bool> dungeon_active;
    std::vector<int> parties_served;
    std::vector<int> total_time_served;
    
    std::random_device rd;
    std::mt19937 gen;
    
    std::atomic<int> total_parties_formed{0};
    std::atomic<int> total_players_added{0};
    bool shutdown{false};

public:
    DungeonManager(int n, int t, int h, int d) 
        : dungeon_count(n), tank_queue(t), healer_queue(h), dps_queue(d), gen(rd()) {
        dungeon_active.resize(n, false);
        parties_served.resize(n, 0);
        total_time_served.resize(n, 0);
    }

    bool canFormParty() {
        return tank_queue >= 1 && healer_queue >= 1 && dps_queue >= 3;
    }

    void formParty() {
        tank_queue -= 1;
        healer_queue -= 1;
        dps_queue -= 3;
        total_parties_formed++;
    }

    void addPlayersToQueue(int tanks, int healers, int dps) {
        tank_queue += tanks;
        healer_queue += healers;
        dps_queue += dps;
        total_players_added += (tanks + healers + dps);
        
        std::cout << "Producer: Added " << tanks << " tanks, " << healers 
                  << " healers, " << dps << " DPS to queue." << std::endl;
        
        cv.notify_all();
    }

    void displayStatus() {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "\n=== Current Instance Status ===" << std::endl;
        for (int i = 0; i < dungeon_count; i++) {
            std::cout << "Instance " << (i + 1) << ": " 
                      << (dungeon_active[i] ? "ACTIVE" : "EMPTY") 
                      << " | Parties served: " << parties_served[i] 
                      << " | Total time: " << total_time_served[i] << "s" << std::endl;
        }
        std::cout << "Players in queue - Tanks: " << tank_queue
                  << ", Healers: " << healer_queue
                  << ", DPS: " << dps_queue << std::endl;
        std::cout << "Total parties formed: " << total_parties_formed << std::endl;
        std::cout << "Total players added: " << total_players_added << std::endl;
        std::cout << "================================\n" << std::endl;
    }

    void dungeonInstance(int instance_id, int t1, int t2) {
        std::uniform_int_distribution<> time_dist(t1, t2);
        
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            
            cv.wait(lock, [this]() { 
                return canFormParty() || shutdown; 
            });
            
            if (shutdown && !canFormParty()) {
                break;
            }
            
            if (canFormParty()) {
                formParty();
                dungeon_active[instance_id] = true;
                parties_served[instance_id]++;
                
                std::cout << "Instance " << (instance_id + 1) 
                          << ": Party formed! Starting dungeon..." << std::endl;
                
                lock.unlock();
                
                int dungeon_time = time_dist(gen);
                std::this_thread::sleep_for(std::chrono::seconds(dungeon_time));
                
                lock.lock();
                total_time_served[instance_id] += dungeon_time;
                dungeon_active[instance_id] = false;
                
                std::cout << "Instance " << (instance_id + 1) 
                          << ": Dungeon completed in " << dungeon_time << " seconds!" << std::endl;
                
                cv.notify_all();
            }
        }
    }

    void playerProducer(int interval_ms, int max_runtime_seconds) {
        std::uniform_int_distribution<> role_dist(0, 2);
        std::uniform_int_distribution<> count_dist(1, 3);
        
        auto start_time = std::chrono::steady_clock::now();
        
        while (true) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
            
            if (elapsed >= max_runtime_seconds) {
                std::cout << "Reached maximum runtime. Stopping producer." << std::endl;
                break;
            }
            
            int tanks_to_add = 0;
            int healers_to_add = 0;
            int dps_to_add = 0;
            
            int players_to_add = count_dist(gen);
            for (int i = 0; i < players_to_add; i++) {
                int role = role_dist(gen);
                switch (role) {
                    case 0: tanks_to_add++; break;
                    case 1: healers_to_add++; break;
                    case 2: dps_to_add++; break;
                }
            }
            
            {
                std::lock_guard<std::mutex> lock(mtx);
                addPlayersToQueue(tanks_to_add, healers_to_add, dps_to_add);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
    }

    void startInstances(int t1, int t2, int producer_interval_ms = 3000, int max_runtime_seconds = 30) {
        std::vector<std::thread> instances;
        
        for (int i = 0; i < dungeon_count; i++) {
            instances.emplace_back(&DungeonManager::dungeonInstance, this, i, t1, t2);
        }
        
        std::thread producer_thread(&DungeonManager::playerProducer, this, producer_interval_ms, max_runtime_seconds);
        
        auto start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(max_runtime_seconds)) {
            displayStatus();
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            shutdown = true;
            cv.notify_all();
        }
        
        producer_thread.join();
        
        for (auto& instance : instances) {
            instance.join();
        }
        
        displayFinalSummary();
    }

    void displayFinalSummary() {
        std::cout << "\n\n=== FINAL SUMMARY ===" << std::endl;
        std::cout << std::setw(12) << "Instance" 
                  << std::setw(15) << "Status" 
                  << std::setw(18) << "Parties Served" 
                  << std::setw(16) << "Total Time" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        
        int total_parties = 0;
        int overall_time = 0;
        
        for (int i = 0; i < dungeon_count; i++) {
            std::cout << std::setw(10) << (i + 1) 
                      << std::setw(15) << (dungeon_active[i] ? "ACTIVE" : "EMPTY")
                      << std::setw(15) << parties_served[i] 
                      << std::setw(15) << total_time_served[i] << "s" << std::endl;
            
            total_parties += parties_served[i];
            overall_time += total_time_served[i];
        }
        
        std::cout << std::string(60, '-') << std::endl;
        std::cout << std::setw(25) << "TOTAL" 
                  << std::setw(15) << total_parties 
                  << std::setw(15) << overall_time << "s" << std::endl;
        std::cout << "Remaining players - Tanks: " << tank_queue
                  << ", Healers: " << healer_queue
                  << ", DPS: " << dps_queue << std::endl;
        std::cout << "Total players added by producer: " << total_players_added << std::endl;
        std::cout << "Total parties formed: " << total_parties_formed << std::endl;
    }
};

bool isValidIntegerInput(const std::string& input) {
    if (input.empty()) {
        return false;
    }
    
    bool foundDigit = false;
    for (char c : input) {
        if (std::isdigit(c)) {
            foundDigit = true;
        } else if (!std::isspace(c)) {
            return false;
        }
    }
    
    return foundDigit;
}

bool isValidNonNegativeInput(const std::string& input) {
    if (input.empty()) {
        return false;
    }
    
    bool foundDigit = false;
    for (char c : input) {
        if (std::isdigit(c)) {
            foundDigit = true;
        } else if (!std::isspace(c)) {
            return false;
        }
    }
    
    return foundDigit;
}

int getValidatedInteger(const std::string& prompt, bool positiveOnly = false) {
    std::string input;
    int value;
    
    while (true) {
        std::cout << prompt;
        std::getline(std::cin, input);
        
        if (!isValidIntegerInput(input)) {
            std::cout << "Invalid input! Please enter a valid integer without letters or special characters." << std::endl;
            continue;
        }
        
        std::stringstream ss(input);
        if (ss >> value) {
            std::string remaining;
            if (ss >> remaining) {
                std::cout << "Invalid input! Please enter only a single integer without extra characters." << std::endl;
                continue;
            }
            
            if (positiveOnly && value <= 0) {
                std::cout << "Invalid input! Please enter a positive integer." << std::endl;
            } else if (!positiveOnly && value < 0) {
                std::cout << "Invalid input! Please enter a non-negative integer." << std::endl;
            } else {
                return value;
            }
        } else {
            std::cout << "Invalid input! Please enter a valid integer." << std::endl;
        }
    }
}

int getValidatedIntegerWithRange(const std::string& prompt, int minValue) {
    std::string input;
    int value;
    
    while (true) {
        std::cout << prompt;
        std::getline(std::cin, input);
        
        if (!isValidIntegerInput(input)) {
            std::cout << "Invalid input! Please enter a valid integer without letters or special characters." << std::endl;
            continue;
        }
        
        std::stringstream ss(input);
        if (ss >> value) {
            std::string remaining;
            if (ss >> remaining) {
                std::cout << "Invalid input! Please enter only a single integer without extra characters." << std::endl;
                continue;
            }
            
            if (value < minValue) {
                std::cout << "Invalid input! Please enter an integer greater than or equal to " << minValue << "." << std::endl;
            } else {
                return value;
            }
        } else {
            std::cout << "Invalid input! Please enter a valid integer." << std::endl;
        }
    }
}

int main() {
    int n, t, h, d, t1, t2;
    
    std::cout << "=== MMORPG Dungeon LFG Queue System ===" << std::endl;
    
    n = getValidatedInteger("Enter number of dungeon instances (n): ", true);
    
    t = getValidatedInteger("Enter initial number of tank players (t): ");
    
    h = getValidatedInteger("Enter initial number of healer players (h): ");
    
    d = getValidatedInteger("Enter initial number of DPS players (d): ");
    
    t1 = getValidatedInteger("Enter minimum dungeon time (t1): ");
    
    t2 = getValidatedIntegerWithRange("Enter maximum dungeon time (t2): ", t1);
    
    std::cout << "\nInitializing dungeon system with:" << std::endl;
    std::cout << "Instances: " << n << " | Initial Tanks: " << t 
              << " | Initial Healers: " << h << " | Initial DPS: " << d << std::endl;
    std::cout << "Dungeon time range: " << t1 << "s to " << t2 << "s" << std::endl;
    std::cout << "Producer will add new players every 3 seconds for 30 seconds." << std::endl;
    
    DungeonManager manager(n, t, h, d);
    manager.startInstances(t1, t2);
    
    return 0;
}