/*
 * demo_uart.h
 * MTRX2700 Lab 2 — Exercise 3: Serial Interface
 *
 * Entry points for each lab sub-task demonstration.
 * Call exactly one of these from main() to exercise the corresponding part.
 *
 * Each function assumes SerialComms_Init() has already been called with a
 * valid SerialPort (e.g. &USART1_PORT).
 *
 * Part | Description
 * -----|------------------------------------------------------------
 *  A   | Raw byte send and blocking string receive
 *  B   | Debug string output via SerialComms_SendString
 *  C   | Structured message TX (text body + SensorData struct)
 *  D   | Structured message RX with polling and callback
 *  E   | Interrupt-driven RX — waits for ISR to fire then reports
 */

#ifndef DEMO_UART_H
#define DEMO_UART_H

/*
 * Demo_RunPartA
 * Sends a fixed byte array then receives 5 bytes into a local buffer.
 * Halts in a spin loop so you can inspect variables in the debugger.
 */
void Demo_RunPartA(void);

/*
 * Demo_RunPartB
 * Continuously sends a debug string with a software delay between each
 * transmission.  Never returns.
 */
void Demo_RunPartB(void);

/*
 * Demo_RunPartC
 * Alternately sends a MSG_TYPE_TEXT message ("HI") and a MSG_TYPE_SENSOR
 * message (heading + button_state struct) with a delay between each.
 * Never returns.
 */
void Demo_RunPartC(void);

/*
 * Demo_RunPartD
 * Registers a receive callback then polls SerialComms_ReceiveMsg() forever.
 * The callback sends "OK GOT IT\r\n" on receipt of any valid packet.
 */
void Demo_RunPartD(void);

/*
 * Demo_RunPartE
 * Sends a prompt string then spins until the USART1 ISR fires at least once
 * (detected via SerialComms_IsrFired()).  Prints a confirmation and halts.
 * Demonstrates that the interrupt path is active without a full message parse.
 */
void Demo_RunPartE(void);

#endif /* DEMO_UART_H */
