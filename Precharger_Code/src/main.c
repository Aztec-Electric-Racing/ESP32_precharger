/**
 * @file main.c
 * @brief Precharger safety mechanism using esp32-s2-saola-1 board and ESPIDF
 * 
 * Refer to github README for more info: https://github.com/Aztec-Electric-Racing/Precharger-Board
 * 
 * @author 
 * Sean Hashem
 * Laith Oraha
 * Kohki Kita
 * @date 2025-04-27
 * 
 * @copyright
 * Copyright (c) 2025 Aztec Electric Racing
 * 
 */

 // --- Required libraries for ESP32 and ESPIDF ---
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

// --- Variables for looping ---
bool isDone = false;                //Set to True after full run through of logic, reset to 0 after esp_restart()
bool postOpto = false;              //Represents if we are past the opto-isolator check in main logic
int state = 0;                      //Controls if main loop should run, set to 1 if SDC is closed and isDone == false
bool fatalErrorShutdown = false;    //Set to true if we lose opto-isolator signal and SDC is open

// --- GPIO Pins ---
#define SDC 17                      //Shutdown circuit end: 1 = open, 0 = closed (Input). NOTE: pin 17 and 18 shorted (Spring 2025)
#define OPTO_ISOLATOR_OUT 4         //Reads signal from HV comparator (Input)
#define HV_UNSWITCHED_RELAY 5       //Relay for turning on ground reference for high voltage (Output)
#define CAPACITOR_CHARGE_RELAY 7    //Relay to start charging capacitor (Output)
#define PRECHARGE_RELAY 6           //Bridges resistor across positive AIR (Output)
#define HV_CONTACTOR_RELAY 8        //Turns on positive AIR (Output)

/**
 * @brief ISR Handler Function.
 * 
 * Called when the shutdown circuit (SDC) state changes.
 * If the SDC is open, all relays are deactivated. 
 * If opto-isolator signal loss is detected after activation, triggers fatal error shutdown.
 *
 * @note This function may cause a full ESP32 restart or trigger deep sleep mode to restart the sequence.
 */
static void IRAM_ATTR gpio_isr_handler(void *arg) 
{
    if(gpio_get_level(SDC) == 1) //if SDC is open
    {
        // Set all relays to zero
        gpio_set_level(HV_UNSWITCHED_RELAY, 0);
        gpio_set_level(CAPACITOR_CHARGE_RELAY, 0);
        gpio_set_level(PRECHARGE_RELAY, 0);
        gpio_set_level(HV_CONTACTOR_RELAY, 0);

        // If OPTO_ISOLATOR_OUT and SDC is open -> Fatal error, shutdown board
        if(gpio_get_level(OPTO_ISOLATOR_OUT) == 1 && postOpto == 1)
        {
            fatalErrorShutdown = true;
            esp_deep_sleep_start(); // Sends board into deep sleep, must be restarted to run again
        }
        else
        {
            // Reset board
            esp_restart(); 
        }
    }
}

/**
 * @brief Configures GPIO pin 17 interrupt.
 * 
 * Sets up the shutdown circuit (SDC) pin to trigger an interrupt 
 * on any signal edge (rising or falling). This allows detection 
 * of SDC open/close events to control the precharge logic.
 */
void setup_gpio_interrupt() 
{
    // Reset pin before start
    gpio_reset_pin(SDC);
    gpio_set_direction(SDC, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_pull_mode(SDC, GPIO_PULLDOWN_ENABLE); 
    gpio_set_intr_type(SDC, GPIO_INTR_ANYEDGE); // Trigger on Falling edge (1 -> 0)
    
    gpio_reset_pin(OPTO_ISOLATOR_OUT);
    gpio_set_direction(OPTO_ISOLATOR_OUT, GPIO_MODE_INPUT);
    gpio_set_pull_mode(OPTO_ISOLATOR_OUT, GPIO_PULLDOWN_ENABLE); 
    gpio_set_intr_type(OPTO_ISOLATOR_OUT, GPIO_INTR_ANYEDGE);

    // Install ISR service
    gpio_install_isr_service(0);

    // Attach ISR handler to TEST_GPIO
    gpio_isr_handler_add(SDC, gpio_isr_handler, (void *)SDC);
    gpio_isr_handler_add(OPTO_ISOLATOR_OUT, gpio_isr_handler, (void *)OPTO_ISOLATOR_OUT); 

    // Enable intrrupt on pin 17
    gpio_intr_enable(SDC);
}

/**
 * @brief Setup for other GPIO pins.
 * 
 * Initialized to: 
 * OPTO_ISOLATOR_OUT: 
 * HV_UNSWITCHED_RELAY: 
 * CAPACITOR_CHARGE_RELAY: 
 * PRECHARGE_RELAY: 
 * HV_CONTACTOR_RELAY:
 *
 * @note [Optional] Additional notes, tips, or warnings.
 */
void pin_setup()
{
    gpio_set_direction(HV_UNSWITCHED_RELAY, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(CAPACITOR_CHARGE_RELAY, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(PRECHARGE_RELAY, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction(HV_CONTACTOR_RELAY, GPIO_MODE_INPUT_OUTPUT);

    //Initialize all outputs to 0
    gpio_set_level(SDC, 0);
    gpio_set_level(HV_UNSWITCHED_RELAY, 0);
    gpio_set_level(CAPACITOR_CHARGE_RELAY, 0);
    gpio_set_level(PRECHARGE_RELAY, 0);
    gpio_set_level(HV_CONTACTOR_RELAY, 0);
}


/**
 * @brief Application entry point.
 * 
 * Sets up the hardware pins and enters the main control loop. 
 * Detects shutdown circuit closure, then safely manages the 
 * precharge sequence for the high voltage system.
 *
 * Idles indefinitely after completion or until a shutdown event occurs.
 */
void app_main(void) 
{
    pin_setup();
    while (1) 
    {
        if(gpio_get_level(SDC) == 0 && isDone == false)
        {
            state = 1;
            setup_gpio_interrupt();
        }

        //Main logic
        if(state == 1)
        {
            printf("Hv unswitched set high\n");
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(HV_UNSWITCHED_RELAY, 1);

            printf("Capacitor charge set high\n");
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(CAPACITOR_CHARGE_RELAY, 1);
    
            printf("Precharge relay set high\n");
            vTaskDelay(pdMS_TO_TICKS(5));
            gpio_set_level(PRECHARGE_RELAY, 1);
            
            printf("Waiting for Opto-Isolator output == 0\n");
            while(gpio_get_level(OPTO_ISOLATOR_OUT)){} // Idles till OPTO_ISOLATOR_OUT = 1
 
            gpio_intr_enable(OPTO_ISOLATOR_OUT); //Start monitoring for changes in opto-isolator 
            postOpto = true; 

            vTaskDelay(pdMS_TO_TICKS(5));
    
            printf("HV contactor set high\n");
            gpio_set_level(HV_CONTACTOR_RELAY, 1);
            vTaskDelay(pdMS_TO_TICKS(5));
    
            printf("HV unswitched and Precharge relays set low\n");
            gpio_set_level(HV_UNSWITCHED_RELAY, 0);
            gpio_set_level(PRECHARGE_RELAY, 0);

            printf("End Logic\n");
            isDone = true;
            state = 0;
        }

       // --- FOR DEBUGGING ---
       
       if(state == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(2000));
            printf("In zero-state, waiting for interrupt\n");
            printf("SDC Input: %d\n", gpio_get_level(SDC));
            printf("Opto-Isolator Input: %d\n", gpio_get_level(OPTO_ISOLATOR_OUT));
            printf("HV Contactor Relay: %d\n", gpio_get_level(HV_CONTACTOR_RELAY));
            printf("HV Unswitched Relay: %d\n", gpio_get_level(HV_UNSWITCHED_RELAY));
            printf("Precharge Relay: %d\n", gpio_get_level(PRECHARGE_RELAY));
            printf("Capacitor Charge Relay: %d\n", gpio_get_level(CAPACITOR_CHARGE_RELAY));
            printf("isDone: %d\n", isDone);
            printf("postOpto: %d\n", postOpto);
            printf("fatalErrorShutdown: %d\n\n", fatalErrorShutdown);
        }  
        
    }
}