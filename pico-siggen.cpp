#include <stdio.h>
#include <iostream>
#include <sstream>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"

#include "AD9850.hpp"
#include "command_processor.hpp"

// Control pins for the AD99850 DDS module.
//
const uint OSC_HZ = AD9850::OSC_HZ;
const uint W_CLK  = 10;
const uint FQ_UD  = 11;
const uint DATA   = 12;
const uint RESET  = 13;

// These are the TX and RX pins for UART1
//
std::optional<uart_inst_t*> uart = std::make_optional(uart1);
const uint UART_TX = 4;
const uint UART_RX = 5;
const uint BAUD    = 9600;

// Constnts to define the CR LF values.
//
#ifndef CRLF
    #define CRLF
    const char LF = 0x0A;
    const char CR = 0x0D;
#endif

/**
 * @brief  Alarm callback.
 * @note   Put your timeout handler code in here.
 */
int64_t alarm_callback(alarm_id_t id, void *user_data) 
{
    return 0;
}

/**
 * @brief  Print the error to the stdout in json format.
 * @param  command  Structure containing the returned error.
 */
void show_error(command_t command)
{
    std::ostringstream os;
    os << 
        R"({)" << 
        R"("command_number":)" << command.command_number << "," 
        R"("error":)"          << R"(")"  << command.error.value() << R"(")" <<
        R"(})";

    if (uart.has_value())
    {
        uart_puts(uart.value(), os.str().c_str());
        uart_putc(uart.value(), LF);
    }
    else
    {
        std::cout << os.str() << std::endl;
    }
}

/**
 * @brief  Acknowledges the given command by pringing the 
 *         current DDS state.
 * @param  command_number   Identifier for command being acked.
 * @param  dds              Current dds from which state is being pulled.
 */
void ack_command(int command_number, AD9850 dds)
{
    std::ostringstream os;
    os << 
        R"({)" << 
        R"("command_number":)" <<  command_number << "," 
        R"("frequency":)"      <<  dds.get_frequency() << ","
        R"("phase":)"          <<  dds.get_phase() << ","
        R"("enable_out":)"     << (dds.get_enabled() ? "true" : "false") <<
        R"(})";

    if (uart.has_value())
    {
        uart_puts(uart.value(), os.str().c_str());
        uart_putc(uart.value(), LF);
    }
    else
    {
        std::cout << os.str() << std::endl;
    }
}

/**
 * @brief  Main method
 */
int main()
{
    stdio_init_all();

    // Period timer is here for future expansion.
    //
    add_alarm_in_ms(2000, alarm_callback, NULL, false);

    // Initialize GPIO pins and UART..
    // Pin 0 is TX, 1 is RX.
    // Pin functions have to be set before calling uart_init 
    // to avoid losing data.
    //
    // The UART has to be enabled in the make file for this
    // to work.
    //
    if (uart.has_value())
    {
        gpio_set_function(UART_TX, UART_FUNCSEL_NUM(uart, UART_TX));
        gpio_set_function(UART_RX, UART_FUNCSEL_NUM(uart, UART_RX));
        uart_init(uart.value(), BAUD);
    }

    // Create an instance of the DDS.
    //
    AD9850 dds(OSC_HZ, W_CLK, FQ_UD, DATA, RESET);
    dds.set_frequency(1000);
    dds.commit();
    
    // Create an instance of the command processor
    // to monitor stdio for incoming commands.
    //
    CommandProcessor command_processor(uart);

    // Enter the processing loop.
    //
    while (true)
    {
        // Process any available commands.
        //
        command_processor.loop();
        if (command_processor.command_is_available())
        {
            command_t command = command_processor.get_command();

            // See if there was an error.  If so, send out
            // json containing the error message and continue
            // back to the top of the loop.
            //
            if (command.error.has_value())
            {
                show_error(command);
                continue;
            }

            // No error.  Process the command contents.
            //
            if (command.frequency_hz.has_value())
            {
                dds.set_frequency(command.frequency_hz.value());
            }

            if (command.phase_deg.has_value())
            {
                dds.set_phase(command.phase_deg.value());
            }

            if (command.enable_out.has_value())
            {
                dds.enable_out(command.enable_out.value());
            }

            dds.commit();

            // Acknowledge the command.
            //
            ack_command(command.command_number, dds);
        }
    }

    return 0;
}
