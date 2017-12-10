//Communicate via Serial connection
#pragma once

#include <EEPROM.h>
#include <Arduino.h>

#include <ArduinoSTL.h>

#include "storage.h"

#include <pb_encode.h>
#include <pb_decode.h>
namespace pb{
    #include "iris.pb.h"
}

namespace freilite{
namespace iris{
namespace communication{
    // Maximum size of nanopb's internal buffer
    const size_t MAX_SIZE_PB_BUFFER = 300;
    // Maximum ms to wait for the next message
    // 2000 seems to be a safe number, 1000 was too low
    const size_t RECEIVE_TIMEOUT = 2000;

    using namespace pb;

    // Print just like std::printf but with leading EOT, see iris.proto for details
    // Should always be used instead of std::printf
    int printf(const char* format, ... ){
        int num_written = std::printf("\x04");
        if(num_written < 1){
            return num_written;
        }
        va_list arglist;
        va_start(arglist, format);
        num_written += std::vprintf(format, arglist);
        va_end(arglist);
        return num_written;
    }

    namespace {
        // Read protobuf data into buffer from serial connection
        static bool read_callback(pb_istream_t* stream,
                                  uint8_t* buf,
                                  size_t count){
            int bytes_read = 0;
            for(;bytes_read < count; ++bytes_read){
                if(!SerialUSB.available()){
                    // Reached end of data
                    buf[bytes_read] = 0;
                    stream->bytes_left = 0;
                } else {
                    // Read data into buffer
                    buf[bytes_read] = SerialUSB.read();
                }
            }

            return count == bytes_read;
        }

        // Decoder for protobuf messages
        pb_istream_t pb_serial_in = {&read_callback,
                                         nullptr,
                                         MAX_SIZE_PB_BUFFER};

        // Write protobuf data onto serial connection
        static bool write_callback(pb_ostream_t* stream,
                                   const uint8_t* buf,
                                   size_t count){
            return SerialUSB.write(buf, count) == count;
        }

        // Encoder for protobuf messages
        pb_ostream_t pb_serial_out = {&write_callback,
                                          nullptr,
                                          MAX_SIZE_PB_BUFFER};
    }

    MessageData receive_message(){
        MessageData message = MessageData_init_default;
        pb_decode(&pb_serial_in,
                          MessageData_fields,
                          &message);
        return message;
    }

    void send_message(const MessageData& message){
        pb_encode(&pb_serial_out,
                          MessageData_fields,
                          &message);
    }

    void send_message(MessageData_Signal signal){
        MessageData message_data = MessageData_init_default;
        message_data.which_content = MessageData_signal_tag;
        message_data.content.signal = signal;

        send_message(message_data);
    }

    void send_message(const pb::Cue& cue){
        MessageData message_data = MessageData_init_default;
        message_data.which_content = MessageData_cue_tag;
        message_data.content.cue = cue;

        send_message(message_data);
    }

    void send_message(const pb::Schedule& schedule){
        MessageData message_data = MessageData_init_default;
        message_data.which_content = MessageData_schedule_tag;
        message_data.content.schedule = schedule;

        send_message(message_data);
    }

    // Check if message is incoming
    // Blocks for no more than RECEIVE_TIMEOUT milliseconds
    bool message_incoming(){
        const size_t TIMESTEP = 3;
        unsigned long milliseconds = 0;
        while(!SerialUSB.available()){
            delay(TIMESTEP);
            milliseconds += TIMESTEP;
            if(milliseconds > RECEIVE_TIMEOUT){
                printf("Timeout reached: %ims", milliseconds);
                return false;
            }
        }
        return true;
    }

    // Wait for the next signal and return true if it's
    bool next_requested(){
        if(!message_incoming()){
            return false;
        }
        return receive_message().content.signal ==
               MessageData_Signal_RequestNext;
    }

    void handle_info(){
        printf("Communication works!");
    }

    void handle_download_configuration(){
        // Send cues
        size_t num_cues = storage::number_of_cues();
        for(int i = 0; i < num_cues; ++i){
            send_message(storage::get_cue(i).as_pb_cue());
            if(!next_requested()){
                printf("Did not receive RequestNext.");
                return;
            }
        }

        // Send schedules
        size_t num_schedules = Schedules::count();
        for(int i = 0; i < num_schedules; ++i){
            send_message(Schedule(i).as_pb_schedule());
            if(!next_requested()){
                printf("Did not receive RequestNext.");
                return;
            }
        }

        // Send Confirm signal to indicate end of transmission
        send_message(MessageData_Signal_Confirm);
    }

    // Handle I/O
    void handle_serial_io(){
        // Don't do anything if there are no incoming requests
        if(!SerialUSB.available()){
            return;
        }

        MessageData request = receive_message();

        if(request.which_content != MessageData_signal_tag){
            send_message(MessageData_Signal_Error);
            return;
        }

        switch(request.content.signal){
            case MessageData_Signal_RequestInfo:
                handle_info();
                return;

            case MessageData_Signal_DownloadConfiguration:
                handle_download_configuration();
                return;

            // Confirmations are always okay
            case MessageData_Signal_Confirm:
                send_message(MessageData_Signal_Confirm);
                return;

            default:
                printf("Signal unexpected, unknown, or not implemented.");
                return;
        }
    }
}
}
}
