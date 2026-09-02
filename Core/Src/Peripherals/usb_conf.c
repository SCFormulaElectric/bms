#include "Peripherals/usb_conf.h"

#include "app.h"
#include "Tasks/Task_Helper/handles.h"

volatile sd_card_owner_t sd_card_owner;

__weak uint8_t CDC_Transmit_FS(uint8_t *buffer, uint16_t length)
{
    (void)buffer;
    (void)length;
    return 1U;
}

void AMS_USB_CDC_Receive(uint8_t *buffer, uint32_t length)
{
    /* A read-only diagnostic protocol will be added after the data model is
     * frozen. Incoming USB bytes currently have no control authority. */
    (void)buffer;
    (void)length;
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
#if AMS_USB_MSC_SD_ENABLED
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (app.task_entries[sd_card_task_index].handle != NULL) {
        xTaskNotifyFromISR(app.task_entries[sd_card_task_index].handle,
            USB_CONNECTED, eSetBits, &higher_priority_task_woken);
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
#endif
    (void)hpcd;
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
#if AMS_USB_MSC_SD_ENABLED
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (app.task_entries[sd_card_task_index].handle != NULL) {
        xTaskNotifyFromISR(app.task_entries[sd_card_task_index].handle,
            USB_DISCONNECTED, eSetBits, &higher_priority_task_woken);
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
#endif
    (void)hpcd;
}
