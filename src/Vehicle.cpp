#include "Vehicle.h"

#include "Coordinate.h"
#include "Passenger.h"

void Vehicle::SetPassenger(std::shared_ptr<Passenger> passenger) {
    passenger_ = passenger;
    // Set passenger's destination as the vehicle's destination
    SetDestination(passenger->GetDestination());
}

void Vehicle::SetPosition(const Coordinate &position) {
    position_ = position;
    // If there is a passenger, match to the vehicle's position
    if (passenger_ != nullptr) {
        passenger_->SetPosition(position);
    }
}

void Vehicle::SetDestination(const Coordinate &destination) {
    destination_ = destination;
    // Reset the path and index so will properly route on a new path
    ResetPathAndIndex();
}

void Vehicle::DropOffPassenger() {
    // Clear out the passenger
    passenger_.reset();
    // Clear out failures as well, since had a successful ride
    failures_ = 0;
    // TODO: May want more post-dropoff later
}

void Vehicle::ResetPathAndIndex() {
    path_.clear();
    path_index_ = 0;
}