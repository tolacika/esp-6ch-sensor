import './style.css'

console.log('Hello Vite + JS!')

window.addEventListener('load', () => {
  console.log('Page fully loaded, starting script...');

  const parseRadios = () => {
    const radios = document.querySelectorAll('input[name^="sensor_ch"]:checked');
    let bitmask = 0;

    radios.forEach((radio) => {
      const match = radio.name.match(/sensor_ch(\d+)/);
      if (match) {
        const index = parseInt(match[1], 10);
        if (radio.value === '1') {
          bitmask |= (1 << index);
        }
      }
    });

    document.querySelector('#sensor_mask').value = bitmask;
    document.querySelector('#sensor_mask_hex').textContent = '0x' + bitmask.toString(16).toUpperCase();
    document.querySelector('#sensor_mask_binary').textContent = '0b' + bitmask.toString(2).padStart(radios.length, '0');
  };

  const fillForm = (data) => {
    console.log('Filling form with data:', data);
    let el = document.querySelector('input[name="sta_ssid"]');
    if (el) {
      el.value = data.sta_ssid || '';
      el.removeAttribute('disabled');
      el.dispatchEvent(new Event('input', { bubbles: true }));
    }

    el = document.querySelector('input[name="sta_pass"]');
    if (el) {
      el.value = data.sta_pass || '';
      el.removeAttribute('disabled');
      el.dispatchEvent(new Event('input', { bubbles: true }));
    }

    el = document.querySelector('input[name="ap_ssid"]');
    if (el) {
      el.value = data.ap_ssid || '';
      el.removeAttribute('disabled');
      el.dispatchEvent(new Event('input', { bubbles: true }));
    }

    el = document.querySelector('input[name="ap_pass"]');
    if (el) {
      el.value = data.ap_pass || '';
      el.removeAttribute('disabled');
      el.dispatchEvent(new Event('input', { bubbles: true }));
    }

    el = document.querySelector('input[name="sensor_mask"]');
    if (el) {
      el.value = data.sensor_mask || '';
      el.removeAttribute('disabled');
      el.dispatchEvent(new Event('input', { bubbles: true }));
    }

    el = document.querySelector('#submit_button');
    if (el) {
      el.removeAttribute('disabled');
    }

    for (let i = 0; i < 6; i++) {
      const isEnabled = (data.sensor_mask & (1 << i)) !== 0;
      const radioOn = document.querySelector(`input[name="sensor_ch${i}"][value="1"]`);
      const radioOff = document.querySelector(`input[name="sensor_ch${i}"][value="0"]`);
      if (radioOn && radioOff) {
        radioOn.checked = isEnabled;
        radioOff.checked = !isEnabled;
      }
    }

    parseRadios();
  };
  
  document.querySelectorAll('input[type="radio"]').forEach((radio) => {
    radio.addEventListener('change', (event) => {
      parseRadios();
    });
  });

  document.querySelectorAll('.show_hide_pass').forEach((el) => {
    el.addEventListener('click', (event) => {
      const input = document.querySelector(event.target.getAttribute('data-for'));
      if (!input) {
        console.error('Input element not found for selector:', event.target.getAttribute('data-for'));
        return;
      }
      console.log('Toggling password visibility for:', input, 'Current type:', input.type);
      if (input.type === 'password') {
        input.type = 'text';
        event.target.textContent = 'Hide';
      } else {
        input.type = 'password';
        event.target.textContent = 'Show';
      }
    });
  });

  if (window.location.hostname !== "localhost") {
    const url = new URL(window.location.protocol + '//' + window.location.host + '/config.json');
    fetch(url)
      .then(response => response.json())
      .then(fillForm)
      .catch(error => console.error('Error fetching config:', error));
  } else {
    const testData = {
      sta_ssid: 'TestSSID',
      sta_pass: 'TestPassword',
      ap_ssid: 'TestAPSSID',
      ap_pass: 'TestAPPassword',
      sensor_mask: 0b00101010
    };
    fillForm(testData);
  }
});