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

    void startInstances(int t1, int t2) {
        std::vector<std::thread> instances;
        
        for (int i = 0; i < dungeon_count; i++) {
            instances.emplace_back(&DungeonManager::dungeonInstance, this, i, t1, t2);
        }
        
        auto start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30)) {
            displayStatus();
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            std::lock_guard<std::mutex> lock(mtx);
            if (tank_queue < 1 || healer_queue < 1 || dps_queue < 3) {
                std::cout << "Not enough players to form more parties. Shutting down..." << std::endl;
                break;
            }
        }
        
        {
            std::lock_guard<std::mutex> lock(mtx);
            shutdown = true;
            cv.notify_all();
        }
        
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
    }
};

int main() {
    int n, t, h, d, t1, t2;
    
    std::cout << "=== MMORPG Dungeon LFG Queue System ===" << std::endl;
    
    while (true) {
        std::cout << "Enter number of dungeon instances (n): ";
        std::cin >> n;
        if (std::cin.fail() || n <= 0) {
            std::cout << "Invalid input! Please reinput number of dungeon instances (n)." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }
    
    while (true) {
        std::cout << "Enter number of tank players (t): ";
        std::cin >> t;
        if (std::cin.fail() || t < 0) {
            std::cout << "Invalid input! Please reinput number of tanks (t)." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }
    
    while (true) {
        std::cout << "Enter number of healer players (h): ";
        std::cin >> h;
        if (std::cin.fail() || h < 0) {
            std::cout << "Invalid input! Please reinput number of healers (h)." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }
    
    while (true) {
        std::cout << "Enter number of DPS players (d): ";
        std::cin >> d;
        if (std::cin.fail() || d < 0) {
            std::cout << "Invalid input! Please reinput number of DPS players (d)." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }
    
    while (true) {
        std::cout << "Enter minimum dungeon time (t1): ";
        std::cin >> t1;
        if (std::cin.fail() || t1 < 0) {
            std::cout << "Invalid input! Please reinput minimum dungeon time (t1)." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }
    
    while (true) {
        std::cout << "Enter maximum dungeon time (t2): ";
        std::cin >> t2;
        if (std::cin.fail() || t2 < t1) {
            std::cout << "Invalid input! Please reinput maximum dungeon time (t2)." << std::endl;
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } else {
            break;
        }
    }
    
    std::cout << "\nInitializing dungeon system with:" << std::endl;
    std::cout << "Instances: " << n << " | Tanks: " << t 
              << " | Healers: " << h << " | DPS: " << d << std::endl;
    std::cout << "Dungeon time range: " << t1 << "s to " << t2 << "s" << std::endl;
    
    DungeonManager manager(n, t, h, d);
    manager.startInstances(t1, t2);
    
    return 0;
}