/* ========================================
 *
 * UICO, 2021
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
*/

#ifndef I2C_GEN_2_H
    #define I2C_GEN_2_H
    
    #ifdef __cplusplus
        extern "C" {
    #endif
    
    // === DEPENDENCIES ========================================================
    
    #ifndef __cplusplus
        #include <stdbool.h>
    #endif
    #include <stdint.h>
    
    
    // === TYPE DEFINES ========================================================
    
    typedef uint16_t (*I2CGen2_RxCallback)(uint8_t const*, uint16_t);
    
    // === FUNCTIONS ===========================================================
    
    void i2cGen2_init(void);
    
    void i2cGen2_registerRxCallback(I2CGen2_RxCallback callback);
    
    bool i2cGen2_processRx(void);
    
    bool i2cGen2_read(uint8_t address, uint8_t data[], uint16_t size);
    
    bool i2cGen2_write(uint8_t address, uint8_t data[], uint16_t size);
    
    bool i2cGen2_writeWithAddressInData(uint8_t data[], uint16_t size);
    
    #ifdef __cplusplus
        }
    #endif
    
#endif // I2C_GEN_2_H


/* [] END OF FILE */
