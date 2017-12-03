//Communicate via Serial connection
#pragma once

#include <EEPROM.h>
#include <Arduino.h>

#include <ArduinoSTL.h>

namespace pb{
    #include <pb_encode.h>
    #include <pb_decode.h>
    #include "iris.pb.h"
}

namespace freilite{
namespace iris{
namespace communication{
    // Maximum size of nanopb's internal buffer
    const size_t MAX_SIZE_PB_BUFFER = 300;

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

    void send_message(MessageData& message){
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

    void handle_info(){
        printf("Communication works!");
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

            // Confirmations are always okay
            case MessageData_Signal_Confirm:
                send_message(MessageData_Signal_Confirm);
                return;

            default:
                send_message(MessageData_Signal_Error);
                return;
        }
    }
}
}
}
