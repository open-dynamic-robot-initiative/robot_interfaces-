#pragma once

#include <fstream>
#include <iostream>

#include <chrono>

#include <mpi_cpp_tools/basic_tools.hpp>
#include <mpi_cpp_tools/dynamical_systems.hpp>
#include <mpi_cpp_tools/math.hpp>

#include <real_time_tools/process_manager.hpp>
#include <real_time_tools/thread.hpp>
#include <real_time_tools/threadsafe/threadsafe_timeseries.hpp>
#include <real_time_tools/timer.hpp>

namespace robot_interfaces
{
class FingerLogger
{
public:
    typedef finger::Action Action;
    typedef finger::Observation Observation;
    typedef finger::Status Status;

    int block_size_;
    long int index_;

    bool destructor_was_called_;
    std::ofstream output_file_;
    std::shared_ptr<robot_interfaces::RobotData<Action, Observation, Status>>
        logger_data_;

    std::string output_file_name_;

    FingerLogger(std::shared_ptr<
                     robot_interfaces::RobotData<Action, Observation, Status>>
                     robot_data,
                 int block_size,
                 std::string filename)
        : logger_data_(robot_data),
          block_size_(block_size),
          output_file_name_(filename),
          destructor_was_called_(false)
    {
        thread_ = std::make_shared<real_time_tools::RealTimeThread>();
    }

    virtual ~FingerLogger()
    {
        destructor_was_called_ = true;
        thread_->join();
    }

    static void *write(void *instance_pointer)
    {
        ((FingerLogger *)(instance_pointer))->write();
        return nullptr;
    }

    void write()
    {
        output_file_.open(output_file_name_);

        output_file_ << "# "
                     << "[Timestamp]"
                     << ","
                     << "(Current Index)"
                     << ","
                     << " {O} Angle J1 "
                     << ","
                     << " {O} Angle J2 "
                     << ","
                     << " {O} Angle J3 "
                     << ","
                     << " {O} Velocity J1 "
                     << ","
                     << " {O} Velocity J2 "
                     << ","
                     << " {O} Velocity J3 "
                     << ","
                     << " {O} Torque J1 "
                     << ","
                     << " {O} Torque J2 "
                     << ","
                     << " {O} Torque J3 "
                     << ","
                     << " {A} Applied J1 "
                     << ","
                     << " {A} Applied J2 "
                     << ","
                     << " {A} Applied J3 "
                     << ","
                     << " {A} Desired J1 "
                     << ","
                     << " {A} Desired J2 "
                     << ","
                     << " {A} Desired J3 "
                     << ","
                     << " Status " << std::endl;

        output_file_.close();

        while (!destructor_was_called_ &&
               !(logger_data_->desired_action->length() > 0))
        {
            real_time_tools::Timer::sleep_until_sec(0.1);
        }

        index_ = logger_data_->observation->newest_timeindex();

        for (long int t = 0; !destructor_was_called_; t++)
        {
            if (index_ + block_size_ ==
                logger_data_->observation->newest_timeindex())
            {
                output_file_.open(output_file_name_, std::ios_base::app);

                auto t1 = std::chrono::high_resolution_clock::now();

                for (long int j = index_; j < index_ + block_size_; j++)
                {
                    try
                    {
                        Observation observation =
                            (*logger_data_->observation)[j];
                        Action applied_action =
                            (*logger_data_->applied_action)[j];
                        Action desired_action =
                            (*logger_data_->desired_action)[j];
                        Status status = (*logger_data_->status)[j];

                        output_file_
                            << std::fixed
                            << logger_data_->observation->timestamp_s(j)
                            << " , " << j << " , " << observation.angle[0]
                            << " , " << observation.angle[1] << " , "
                            << observation.angle[2] << " , "
                            << observation.velocity[0] << " , "
                            << observation.velocity[1] << " , "
                            << observation.velocity[2] << " , "
                            << observation.torque[0] << " , "
                            << observation.torque[1] << " , "
                            << observation.torque[2] << " , "
                            << applied_action[0] << " , " << applied_action[1]
                            << " , " << applied_action[2] << " , "
                            << desired_action[0] << " , " << desired_action[1]
                            << " , " << desired_action[2] << " , "
                            << status.action_repetitions << std::endl;
                    }

                    catch (const std::exception &e)
                    {
                        std::cout << "Trying to access index older than the "
                                     "oldest! Skipping ahead."
                                  << std::endl;
                    }

// to check whether the data being requested to be logged is in the buffer of
// the timeseries and inspect effect of delays
#ifdef VERBOSE
                    std::cout << "Index trying to access, oldest index in the "
                                 "buffer: "
                              << j << ","
                              << logger_data_->observation->oldest_timeindex()
                              << std::endl;
#endif
                }

                auto t2 = std::chrono::high_resolution_clock::now();
                auto duration =
                    std::chrono::duration_cast<std::chrono::microseconds>(t2 -
                                                                          t1)
                        .count();

// to print the time taken for one block of data to be logged.
#ifdef VERBOSE
                std::cout << "Time taken for one block of data to be logged: "
                          << duration << std::endl;
#endif

                index_ += block_size_;

                output_file_.close();
            }
        }
    }

    void run()
    {
        thread_->create_realtime_thread(&FingerLogger::write, this);
    }

private:
    std::shared_ptr<real_time_tools::RealTimeThread> thread_;
};

}  // namespace robot_interfaces