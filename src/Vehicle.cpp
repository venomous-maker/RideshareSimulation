#include "Vehicle.h"

#include "Coordinate.h"
#include "Passenger.h"

void Vehicle::SetPassenger(std::shared_ptr<Passenger> passenger) {
    passenger_ = passenger;
    // Set passenger's destination as the vehicle's destination
    auto new_dest = passenger->GetDestination();
    destination_ = new_dest;
}

void Vehicle::SetPosition(const Coordinate &position) {
    position_ = position;
    // If there is a passenger, match to the vehicle's position
    if (passenger_ != nullptr) {
        passenger_->SetPosition(position);
    }
}

void Vehicle::ResetPathAndIndex() {
    path_.clear();
    path_index_ = 0;
}