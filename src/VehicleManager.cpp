#include "VehicleManager.h"
#include <cmath>

#include "Coordinate.h"
#include "Passenger.h"
#include "RoutePlanner.h"
#include "RideMatcher.h"
#include "Vehicle.h"

VehicleManager::VehicleManager(RouteModel *model) : ConcurrentObject(model) {
    // TODO: Add simulation instead of creating vehicles at start?
    for (int i = 0; i < MAX_OBJECTS; ++i) {
        GenerateNew();
    }
    // Set distance per cycle based on model's latitudes
    distance_per_cycle_ = std::abs(model_->MaxLat() - model->MinLat()) / 1000.0;
}

void VehicleManager::GenerateNew() {
    // Get random start position
    auto start = model_->GetRandomMapPosition();
    // Set a random destination until they have a passenger to go pick up
    auto destination = model_->GetRandomMapPosition();
    // Find the nearest road node to start and destination positions
    auto nearest_start = model_->FindClosestNode(start);
    auto nearest_dest = model_->FindClosestNode(destination);
    // Set road position, destination and id of vehicle
    std::shared_ptr<Vehicle> vehicle = std::make_shared<Vehicle>();
    vehicle->SetPosition((Coordinate){.x = nearest_start.x, .y = nearest_start.y});
    vehicle->SetDestination((Coordinate){.x = nearest_dest.x, .y = nearest_dest.y});
    vehicle->SetId(idCnt_++);
    vehicles_.emplace_back(vehicle);
    // Output id and location of vehicle looking to give rides
    std::lock_guard<std::mutex> lck(mtx_);
    std::cout << "Vehicle ID#" << idCnt_ - 1 << " now driving from: " << nearest_start.y << ", " << nearest_start.x << "." << std::endl;
}

void VehicleManager::ResetVehicleDestination(std::shared_ptr<Vehicle> vehicle, bool random) {
    Coordinate destination;
    // Depending on `random`, either get a new random position or set current destination onto nearest node
    if (random) {
        destination = model_->GetRandomMapPosition();
    } else {
        destination = vehicle->GetDestination();
    }
    auto nearest_dest = model_->FindClosestNode(destination);
    vehicle->SetDestination((Coordinate){.x = nearest_dest.x, .y = nearest_dest.y});
    // Reset the path and index so will properly route on a new path
    vehicle->ResetPathAndIndex();
}

// Make for smooth, incremental driving between path nodes
void VehicleManager::IncrementalMove(std::shared_ptr<Vehicle> vehicle) {
    // Check distance to next position vs. distance can go b/w timesteps
    auto pos = vehicle->GetPosition();
    auto next_pos = vehicle->Path().at(vehicle->PathIndex());
    auto distance = std::sqrt(std::pow(next_pos.x - pos.x, 2) + std::pow(next_pos.y - pos.y, 2));

    if (distance <= distance_per_cycle_) {
        // Don't need to calculate intermediate point, just set position as next_pos
        vehicle->SetPosition((Coordinate){.x = next_pos.x, .y = next_pos.y});
        vehicle->IncrementPathIndex();
    } else {
        // Calculate an intermediate position
        double angle = std::atan2(next_pos.y - pos.y, next_pos.x - pos.x); // angle from x-axis
        double new_pos_x = pos.x + (distance_per_cycle_ * std::cos(angle));
        double new_pos_y = pos.y + (distance_per_cycle_ * std::sin(angle));
        vehicle->SetPosition((Coordinate){.x = new_pos_x, .y = new_pos_y});
    }
}

void VehicleManager::Simulate() {
    // Launch Drive function in a thread
    threads.emplace_back(std::thread(&VehicleManager::Drive, this));
}

void VehicleManager::Drive() {
    // Create the route planner to use throughout the sim
    RoutePlanner route_planner = RoutePlanner(*model_);

    while (true) {
        // Sleep at every iteration to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        for (auto &vehicle : vehicles_) {
            // Get a route if none yet given
            if (vehicle->Path().empty()) {
                route_planner.AStarSearch(vehicle);
                // TODO: Replace/remove below when handling impossible routes (i.e. stuck)
                // TODO: Handle below if holding a passenger
                if (vehicle->Path().empty()) {
                    ResetVehicleDestination(vehicle, true);
                    continue;
                }
            }

            // Request a passenger if don't have one yet
            if (vehicle->State() == VehicleState::no_passenger_requested) {
                RequestPassenger(vehicle);
            }

            // Drive to destination or wait, depending on state
            if (vehicle->State() == VehicleState::waiting) {
                continue;
            } else {
                // Drive to current destination
                IncrementalMove(vehicle);
            }

            // Check if at destination
            // TODO: Ensure position and destination ensured to actually match?
            if (vehicle->GetPosition() == vehicle->GetDestination()) {
                if (vehicle->State() == VehicleState::no_passenger_queued) {
                    // Find a new random destination
                    ResetVehicleDestination(vehicle, true);
                } else if (vehicle->State() == VehicleState::passenger_queued) {
                    // Notify of arrival
                    ArrivedAtPassenger(vehicle);
                } else if (vehicle->State() == VehicleState::driving_passenger) {
                    // Drop-off passenger
                    DropOffPassenger(vehicle);
                }
            }
        }
    }
}

void VehicleManager::RequestPassenger(std::shared_ptr<Vehicle> vehicle) {
    // Update state first (make sure no async issues)
    vehicle->SetState(VehicleState::no_passenger_queued);
    // Request the passenger from the ride matcher
    if (ride_matcher_ != nullptr) {
        ride_matcher_->VehicleRequestsPassenger(vehicle->Id());
    }
    // Output notice to console
    std::lock_guard<std::mutex> lck(mtx_);
    std::cout << "Vehicle ID#" << vehicle->Id() << " has requested to be matched with a passenger." << std::endl;
}

void VehicleManager::AssignPassenger(int id, Coordinate position) {
    auto vehicle = vehicles_.at(id);
    // Set new vehicle destination and update its state
    vehicle->SetDestination(position);
    ResetVehicleDestination(vehicle, false); // Aligns to route node and resets path and index
    // Update state when done processing
    vehicle->SetState(VehicleState::passenger_queued);
}

void VehicleManager::ArrivedAtPassenger(std::shared_ptr<Vehicle> vehicle) {
    // Transition to waiting
    vehicle->SetState(VehicleState::waiting);
    // Notify ride matcher
    ride_matcher_->VehicleHasArrived(vehicle->Id());
}

void VehicleManager::PassengerIntoVehicle(int id, std::shared_ptr<Passenger> passenger) {
    auto vehicle = vehicles_.at(id);
    // Set passenger into vehicle and updates its state
    vehicle->SetPassenger(passenger); // Vehicle handles setting new destination with passenger
    ResetVehicleDestination(vehicle, false); // Aligns to route node and resets path and index
    // Update state when done processing
    vehicle->SetState(VehicleState::driving_passenger);
}

void VehicleManager::DropOffPassenger(std::shared_ptr<Vehicle> vehicle) {
    // Output notice to console
    std::unique_lock<std::mutex> lck(mtx_);
    std::cout << "Vehicle ID#" << vehicle->Id() << " has dropped off Passenger ID#" << vehicle->GetPassenger()->Id() << "." << std::endl;
    lck.unlock();
    // Drop off the passenger
    vehicle->DropOffPassenger();
    // Find a new random destination
    ResetVehicleDestination(vehicle, true);
    // Transition back to no passenger requested state
    vehicle->SetState(VehicleState::no_passenger_requested);
}