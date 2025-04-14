#include "nvs_manager.h"
#include "esp_log.h"

esp_err_t store_string(const char *key, const char *value)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
  if (err == ESP_OK)
  {
    err = nvs_set_str(nvs_handle, key, value);
    if (err == ESP_OK)
    {
      nvs_commit(nvs_handle);
    }
    else
    {
      ESP_LOGE("NVS", "Error storing string: %s", esp_err_to_name(err));
    }
    nvs_close(nvs_handle);
  }
  else
  {
    ESP_LOGE("NVS", "Error opening NVS: %s", esp_err_to_name(err));
  }

  return err;
}

esp_err_t read_string(const char *key, char *value, size_t *max_len)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
  if (err == ESP_OK)
  {
    size_t required_size;
    err = nvs_get_str(nvs_handle, key, NULL, &required_size);
    if (err == ESP_OK)
    {
      if (required_size <= *max_len)
      {
        err = nvs_get_str(nvs_handle, key, value, &required_size);
        if (err == ESP_OK)
        {
          value[required_size - 1] = '\0'; // Ensure leading null terminator
          *max_len = required_size;       // Return the actual size
        }
      }
      else
      {
        nvs_close(nvs_handle);
        return ESP_ERR_NVS_INVALID_LENGTH;
      }
    }
    else
    {
      nvs_close(nvs_handle);
      return err;
    }
    nvs_close(nvs_handle);
  }
  else
  {
    return err;
  }

  return ESP_OK;
}

esp_err_t store_int(const char *key, int32_t value)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
  if (err == ESP_OK)
  {
    err = nvs_set_i32(nvs_handle, key, value);
    if (err == ESP_OK)
    {
      nvs_commit(nvs_handle);
    }
    else
    {
      return err;
    }
    nvs_close(nvs_handle);
  }
  else
  {
    return err;
  }

  return ESP_OK;
}

esp_err_t read_int(const char *key, int32_t *value)
{
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open("storage", NVS_READONLY, &nvs_handle);
  if (err == ESP_OK)
  {
    nvs_get_i32(nvs_handle, key, value);
    nvs_close(nvs_handle);
  }
  else
  {
    return err;
  }

  return ESP_OK;
}
