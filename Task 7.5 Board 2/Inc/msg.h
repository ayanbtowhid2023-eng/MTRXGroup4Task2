/**
 * @file msg.h
 * @brief Shared message definition for Exercise 5 integration task.
 *
 * Included by BOTH Board 1 and Board 2 projects.
 *
 * Packet format (from Ex3 serial module):
 *   [ START(0x02) | SIZE | TYPE | BODY... | STOP(0x03) | CHECKSUM ]
 *
 * Body = MagMessage_t
 *   heading_deg  : compass heading 0.0 - 360.0 degrees
 *   button_flag  : 0 = servo mode, 1 = LED mode (toggles on each button press)
 */

#ifndef MSG_H
#define MSG_H

#include <stdint.h>

/* Message type code used in the TYPE field of the packet */
#define MSG_TYPE_MAG  0x01

/**
 * @brief Body of the magnetometer message sent from Board 1 to Board 2.
 *
 * heading_deg : decoded compass heading in degrees (0.0 to 360.0)
 * button_flag : display mode — 0 = servo, 1 = LEDs
 *               toggled each time the button on Board 1 is pressed
 */
typedef struct {
    float   heading_deg;
    uint8_t button_flag;
} MagMessage_t;

#endif /* MSG_H */
