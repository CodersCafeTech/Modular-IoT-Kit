let chartData = {};
let gaugeData ={};


window.addEventListener('DOMContentLoaded', function() {
  const container = document.querySelector('.container');
  const editButton = document.getElementById('edit-button');
  const addButton = document.getElementById('add-button');
  const widgetList = document.getElementById('widget-list');
  const widgetLabels = document.querySelectorAll('.widget-label');
  const userlink = document.getElementById('userlink-button');

  userlink.addEventListener('click', displayUserForm);

  const widgetIds = new Set(); // Set to store added widget IDs
  const div_html = {
    btn__card: `<div class="linkicon"><div class="icon__link" id="link-button">
      <ion-icon name="link"></ion-icon>
    </div></div>
    <div class="switch" id="card__button"><div class="switch__2"><input id="switch-2" type="checkbox"><label for="switch-2"></label></div><div class="name">Button</div></div>`,
    sensor__card: `<div class="linkicon"><div class="icon__link" id="link-button">
    <ion-icon name="link"></ion-icon>
  </div></div>
  <div class="value-unit">
    <div class="value">0</div>
    <div class="unit">U</div>
  </div>
  <div class="name">Sensor Name</div>`,
    slider__card: `<div class="linkicon"><div class="icon__link" id="link-button">
    <ion-icon name="link"></ion-icon>
  </div></div>
  <div class="slider__widget">
      <div class="slider__box">
          <span class="slider__btn" id="find"></span>
          <span class="slider__color"></span>
          <span class="slider__tooltip">50%</span>
      </div>
  <div class="name">Slider Name</div>
  </div>`,
    chart__card:`<div class="linkicon"><div class="icon__link" id="link-button">
    <ion-icon name="link"></ion-icon>
  </div></div><div class="name">Name</div><canvas class="chart"></canvas>`,
  gauge__card: `<div class="linkicon">
    <div class="icon__link" id="link-button">
      <ion-icon name="link"></ion-icon>
    </div>
  </div>
  <div class="gauge">
    <canvas class="gauge__canvas" width="100px" height="100px"></canvas>
    <div class="gauge__value">0</div>
    <div class="name">Gauge</div>
  </div>`,
  alert__card: `<div class="linkicon"><div class="icon__link" id="link-button">
  <ion-icon name="link"></ion-icon>
</div></div>
<div class="lottie">
<lottie-player id="lottie" src="https://lottie.host/d7451ffe-ec0e-4541-a8d8-5e9b4771d291/IPHThhefqJ.json"></lottie-player>
</div>
<div class="name">Sensor Name</div>`,
  };

  let isEditMode = false;
  let isAddMode = false;


  function createWidgetClone(widgetLabel) {
    const widget = document.createElement('div');

    const widgetID = widgetLabel.id; // Get the ID of the clicked widget label
    widget.className = widgetID; // Use the ID as the class of the widget

    widget.innerHTML = div_html[widgetID];

    const clone = widget.cloneNode(true);

    // Generate a unique ID for the widget clone
    const widgetCloneId = `widget-${Date.now()}`;

    clone.setAttribute('data-id', widgetCloneId); // Set widget ID as data attribute

    const switchButton = clone.querySelector('.switch__2 input');
    if (switchButton){
    switchButton.addEventListener('change', function(event) {
      const isChecked = event.target.checked;
      const widget__id = clone.getAttribute('widget__id');

      if (isChecked) {
        const message = `${widget__id}:1`; // Customize the message content as needed
        publishMessage(message); // Call the function to publish the message to MQTT
        console.log("On");
      } else {
        const message = `${widget__id}:0`; // Customize the message content as needed
        publishMessage(message); // Call the function to publish the message to MQTT
      }
    });
    }

    if (clone.classList.contains('slider__card')) {
      const sliderWidget = clone.querySelector('.slider__widget');
      const sliderBox = sliderWidget.querySelector('.slider__box');
      const sliderBtn = sliderWidget.querySelector('.slider__btn');
  
      dragElement(sliderBox, sliderBtn);
    }
    return clone;
  }

  function removeWidget(widget) {
    if (!isEditMode) return; // Only remove widget in Edit mode
    widget.remove();
    const widgetId = widget.getAttribute('data-id'); // Get widget ID
    widgetIds.delete(widgetId); // Remove widget ID from the set
    saveContainerState();
  }

  widgetLabels.forEach(function(widgetLabel) {
    widgetLabel.addEventListener('click', function(event) {
      if (!isAddMode) return;

      const widgetLabel = event.target;
      const clone = createWidgetClone(widgetLabel);
      container.appendChild(clone);
      widgetIds.add(clone.getAttribute('data-id'));

      if (clone.classList.contains('chart__card')) {
        const chartCanvas = clone.querySelector('canvas.chart');
        const widget__id = clone.getAttribute('widget__id');
        const chart = new Chart(chartCanvas, {
          type: 'line',
          data: {
            labels: [],
            datasets: [{
              label: '',
              backgroundColor: 'rgb(255, 99, 132)',
              borderColor: 'rgb(255, 99, 132)',
              data: [],
            }],
          },
          options: {
            plugins: {
              legend: {
                display: true,
                labels: {
                  color: 'rgb(255, 99, 132)',
                  boxHeight: 0,
                  boxWidth: 0,
                },
              },
            },
          },
        });
        chartData[widget__id] = chart;
      };

      if (clone.classList.contains('btn__card')) {
        const switchButton = clone.querySelector('.switch__2 input');
        switchButton.addEventListener('change', function(event) {
          const isChecked = event.target.checked;
          const widget__id = clone.getAttribute('widget__id');
  
          if (isChecked) {
            const message = `${widget__id}:1`; // Customize the message content as needed
            publishMessage(message); // Call the function to publish the message to MQTT
            console.log(message);
          } else {
            const message = `${widget__id}:0`; // Customize the message content as needed
            publishMessage(message); // Call the function to publish the message to MQTT
            console.log(message);
          }
        });
      }

      if (clone.classList.contains('gauge__card')) {
        // Add Gauge widget-specific initialization here
        initializeGauge(clone);
      }


      saveContainerState();
    });
  });

  function toggleAddMode() {
    isAddMode = !isAddMode;
    const dashboard = document.querySelector('.dashboard');
    const sidebar = document.querySelector('.sidebar');
    const widgetList = document.querySelector('.widget-list');

    dashboard.classList.toggle('add-mode');
    sidebar.classList.toggle('add-mode');
    if (isAddMode) {
      interact('.widget-label').unset();
      widgetList.style.display = 'block';
      sidebar.style.display = 'block';
      addButton.innerHTML = '<ion-icon name="checkmark"></ion-icon>';
    } else {
      sidebar.style.display = 'none';
      addButton.innerHTML = '<ion-icon name="add"></ion-icon>';
    }
  }

  function toggleEditMode() {
    isEditMode = !isEditMode;
    const dashboard = document.querySelector('.dashboard');
    const sidebar = document.querySelector('.sidebar');
    const linkIcons = document.querySelectorAll('.linkicon');
    const form__container = document.querySelector('.form__container')

    dashboard.classList.toggle('edit-mode');
    sidebar.classList.toggle('edit-mode');
    if (isEditMode) {
      editButton.innerHTML = '<ion-icon name="checkmark"></ion-icon>';
      linkIcons.forEach(linkIcon => {
        linkIcon.style.display = 'block';
        linkIcon.addEventListener('click', displayForm);
      });
      interact('.slider__box').draggable(false);
      interact('.widget, .btn__card, .sensor__card, .slider__card, .chart__card, .gauge__card, .alert__card')
        .draggable({
          inertia: true,
          modifiers: [
            interact.modifiers.restrictRect({
              restriction: container,
              endOnly: true,
            }),
          ],
          autoScroll: true,
          listeners: {
            move: function(event) {
              const target = event.target;
              const x = (parseFloat(target.getAttribute('data-x')) || 0) + event.dx;
              const y = (parseFloat(target.getAttribute('data-y')) || 0) + event.dy;

              target.style.transform = `translate(${x}px, ${y}px)`;
              target.setAttribute('data-x', x);
              target.setAttribute('data-y', y);
              saveContainerState();
            },
          },
        })
        .resizable({
          edges: { left: true, right: true, bottom: true, top: true },
          listeners: {
            move: function(event) {
              const target = event.target;
              const x = parseFloat(target.getAttribute('data-x')) || 0;
              const y = parseFloat(target.getAttribute('data-y')) || 0;

              target.style.width = `${event.rect.width}px`;
              target.style.height = `${event.rect.height}px`;

              target.setAttribute('data-x', x);
              target.setAttribute('data-y', y);
              saveContainerState();
            },
          },
        })
        .on('doubletap', function(event) {
          const widget = event.target;
          removeWidget(widget);
        });
    } else {
      interact('.widget, .btn__card, .sensor__card, .slider__card, .chart__card, .gauge__card').draggable(false).resizable(false);
      interact('.slider__tooltip, .slider__box').draggable(true).resizable(false);
      sidebar.style.display = 'none';
      form__container.style.display= 'none';
      editButton.innerHTML = '<ion-icon name="create"></ion-icon>';
      linkIcons.forEach(linkIcon => {
        linkIcon.style.display = 'none';
      });
      saveContainerState();
    }
  }

  function saveContainerState() {
    const kit_id = container.getAttribute('kit_id');
    const widgets = Array.from(container.children).map(function(widget) {
      const widgetId = widget.getAttribute('data-id');
      const widgetClass = widget.classList[0]; // Get the class (excluding 'widget')
      const html = widget.innerHTML;
      const x = (parseFloat(widget.getAttribute('data-x')) || 0);
      const y = (parseFloat(widget.getAttribute('data-y')) || 0);
      const width = widget.clientWidth;
      const height = widget.clientHeight;
      const widget__id = widget.getAttribute('widget__id'); // Get the widget__id attribute

      return {
        id: widgetId,
        class: widgetClass,
        html: html,
        x: x,
        y: y,
        width: width,
        height: height,
        widget__id: widget__id, // Include widget__id in the state
      };
    });

    const state = {
      widgets: widgets,
      isEditMode: isEditMode,
      kit_id: kit_id,
    };
    localStorage.setItem('containerState', JSON.stringify(state));
  }

  addButton.addEventListener('click', function() {
    if (isAddMode) {
      saveContainerState();
      toggleAddMode();
    } else {
      toggleAddMode();
    }
  });

  editButton.addEventListener('click', function() {
    if (isEditMode) {
      saveContainerState();
      toggleEditMode();
    } else {
      toggleEditMode();
    }
  });

  function restoreContainerState() {
    const savedState = localStorage.getItem('containerState');

    if (savedState) {
      const state = JSON.parse(savedState);
      isEditMode = state.isEditMode;
      container.setAttribute('kit_id', state.kit_id);

      state.widgets.forEach(function(widgetState) {
        const widget = document.createElement('div');

        widget.className = `${widgetState.class}`;
        widget.innerHTML = widgetState.html;
        widget.style.transform = `translate(${widgetState.x}px, ${widgetState.y}px)`;
        widget.style.width = `${widgetState.width}px`;
        widget.style.height = `${widgetState.height}px`;
        widget.setAttribute('data-id', widgetState.id);
        widget.setAttribute('data-x', widgetState.x);
        widget.setAttribute('data-y', widgetState.y);
        widget.setAttribute('widget__id', widgetState.widget__id); // Set widget__id attribute

        container.appendChild(widget);
        widgetIds.add(widgetState.id);

        if (widgetState.class === 'slider__card') {
          const sliderWidget = widget.querySelector('.slider__widget');
          const sliderBox = sliderWidget.querySelector('.slider__box');
          const sliderBtn = sliderWidget.querySelector('.slider__btn');
    
          // Enable dragging for each slider widget
          dragElement(sliderBox, sliderBtn);
        }

        if (widgetState.class === 'btn__card') {
          const switchButton = widget.querySelector('.switch__2 input');
          switchButton.addEventListener('change', function(event) {
            const isChecked = event.target.checked;
            const widget__id = widget.getAttribute('widget__id');
  
            if (isChecked) {
              const message = `${widget__id}:1`; // Customize the message content as needed
              publishMessage(message); // Call the function to publish the message to MQTT
            } else {
              const message = `${widget__id}:0`; // Customize the message content as needed
              publishMessage(message); // Call the function to publish the message to MQTT
            }
          });
        }

        if (widgetState.class === 'chart__card') {
          const chartCanvas = widget.querySelector('canvas.chart');
          const widget__id = widget.getAttribute('widget__id');
          chartCanvas.setAttribute('id', `chart-${widget__id}`);
          const chart = new Chart(chartCanvas, {
            type: 'line',
            data: {
              labels: [],
              datasets: [{
                label: '',
                backgroundColor: 'rgb(255, 99, 132)',
                borderColor: 'rgb(255, 99, 132)',
                data: [],
              }],
            },
            options: {
              plugins: {
                legend: {
                  display: true,
                  labels: {
                    color: 'rgb(255, 99, 132)',
                    boxHeight: 0,
                    boxWidth: 0,
                  },
                },
              },
            },
          });
          chartData[widget__id] = chart;
        }

      });

      const gaugeWidgets = document.querySelectorAll('.gauge__card');
      gaugeWidgets.forEach(initializeGauge);


      if (isEditMode) {
        toggleEditMode();
      }

      chartData = state.chartData || {};

      const savedUserName = localStorage.getItem('user_name');
      if (savedUserName) {
        const userNameElement = document.getElementById('user_name');
        userNameElement.innerHTML = savedUserName + '\'s Dashboard';
      }
    }
  }

  // Call the restoreContainerState function on page load
  restoreContainerState();

  // slider
  const box = document.querySelector('.slider__box');
  const btn = document.querySelector('.slider__btn');
  const color = document.querySelector('.slider__color');
  const tooltip = document.querySelector('.slider__tooltip');

  let onMouseMove = () => {};

  function dragElement(target, btn) {
    if (!target || !btn) {
      return; // Exit the function if either target or btn is missing
    }
  
    let btnX = 0;
    let mouseX = 0;
  
    target.addEventListener('mousedown', onMouseDown);
  
    function onMouseDown(event) {
      event.preventDefault();
      btnX = btn.offsetLeft;
      mouseX = event.clientX;
  
      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    }
  
    function onMouseMove(event) {
      const dx = event.clientX - mouseX;
      const newBtnX = btnX + dx;
  
      const minBtnX = 0;
      const maxBtnX = target.offsetWidth - btn.offsetWidth;
      const clampedBtnX = Math.max(minBtnX, Math.min(newBtnX, maxBtnX));
  
      btn.style.left = clampedBtnX + 'px';
  
      const percentPosition = (clampedBtnX / maxBtnX) * 100;
  
      const color = target.querySelector('.slider__color');
      if (color) {
        color.style.width = percentPosition + '%';
      }
  
      const tooltip = target.querySelector('.slider__tooltip');
      if (tooltip) {
        tooltip.style.left = clampedBtnX - 5 + 'px';
        tooltip.textContent = Math.round(percentPosition) + '%';
      }
    }
  
    function onMouseUp() {
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
    }
  }
  

  dragElement(box, btn);

  // Link Button
  function displayForm(event) {
    const formContainer = document.getElementById('form__container');
    const unitContainer = formContainer.querySelector('#unit__container');
    console.log(unitContainer);
    // Store a reference to the widget that the link icon belongs to
    const widget = event.target.closest('div[class$="__card"]');
    if (widget.classList.contains('sensor__card')){
      unitContainer.style.display = 'block';
    }
    else{
      unitContainer.style.display = 'none';
    }

    formContainer.style.display = 'block';

    // Attach submit event listener to the form
    const form = document.getElementById('widget-form');
    const submit = document.getElementById('submit');

    submit.addEventListener('click', function(event) {
      event.preventDefault(); // Prevent the default form submission
      form.dispatchEvent(new Event('submit')); // Programmatically trigger the form submit event
    });

    form.addEventListener('submit', function(e) {
      e.preventDefault();

      // Retrieve the entered values from the form fields
      const name = document.getElementById('display__name').value;
      const sensor__id = document.getElementById('sensor__id').value;
      const unit = document.getElementById('unit__id').value;
      // Retrieve more input values as needed
      // Add the entered values to the widget
      addValuesToWidget(widget, name, sensor__id, unit);
      saveContainerState();

      // Reset the form and hide it
      form.reset();
      formContainer.style.display = 'none';
      //container.innerHTML = '';
      //restoreContainerState();
      location.reload();
    });
  }

  // Function to add the entered values to the widget
  function addValuesToWidget(widget, name, sensor__id, unit) {
    const displayName = widget.querySelector('.name');
    displayName.textContent = name;

    if (unit) {
      const displayUnit = widget.querySelector('.unit');
      displayUnit.textContent = unit;
    }
    widget.setAttribute('widget__id', sensor__id);
  }

  // User Linking
  function displayUserForm(event) {
    const userContainer = document.getElementById('user__container');

    if (userContainer.style.display === 'block'){
      userContainer.style.display = 'none';
    }
    else{
      userContainer.style.display = 'block'
    }

    // Attach submit event listener to the form
    const form = document.getElementById('user__form');
    const submit = document.getElementById('user__submit');

    submit.addEventListener('click', function(event) {
      event.preventDefault(); // Prevent the default form submission
      form.dispatchEvent(new Event('submit')); // Programmatically trigger the form submit event
    });

    form.addEventListener('submit', function(e) {
      e.preventDefault();

      // Retrieve the entered values from the form fields
      const user__name = document.getElementById('user__name').value;
      const kit__id = document.getElementById('kit__id').value;
      // Retrieve more input values as needed
      // Add the entered values to the widget
      addValuesToContainer(user__name, kit__id);

      // Reset the form and hide it
      form.reset();
      userContainer.style.display = 'none';
    });
  }

  // Function to add the entered values to the widget
  function addValuesToContainer(username, kit__id) {
    const userName = document.getElementById('user_name');
    localStorage.setItem('user_name', username);
    username = username + '\'s Dashboard';
    userName.innerHTML = username;
    container.setAttribute('kit_id', kit__id);
    saveContainerState();
  }


  function addDataToChart(widget__id, label, data) {
    let chart = chartData[widget__id];
    if (!chart) {
      const chartCanvas = document.getElementById(`chart-${widget__id}`);
      const existingChart = Chart.getChart(chartCanvas);
      if (existingChart) {
        existingChart.destroy(); // Destroy the previous Chart instance
      }
      const chartContext = chartCanvas.getContext('2d');
      chart = new Chart(chartCanvas, {
        type: 'line',
        data: {
          labels: [],
          datasets: [{
            label: '',
            backgroundColor: 'rgb(255, 99, 132)',
            borderColor: 'rgb(255, 99, 132)',
            data: [],
          }],
        },
        options: {
          plugins: {
            legend: {
              display: true,
              labels: {
                color: 'rgb(255, 99, 132)',
                boxHeight: 0,
                boxWidth: 0,
              },
            },
          },
        },
      });
      chartData[widget__id] = chart;
    }

    // Update the chart data
    chart.data.labels.push(label);
    chart.data.datasets[0].data.push(data);
    chart.update();
  }

  function updateGaugeValue(widgetID, value) {
    const gaugeWidgetList = document.querySelectorAll(`div[widget__id="${widgetID}"]`);
    gaugeWidgetList.forEach((element) => {
      const gaugeCanvas = element.querySelector('.gauge__canvas');
      if (gaugeCanvas){
      const gauge = gaugeCanvas.gauge;
      const gaugeValueElement = gaugeCanvas.nextElementSibling; // Assuming .gauge__value is the next sibling
      if (gaugeValueElement.classList.contains('gauge__value')) {
        gaugeValueElement.innerHTML = value;
      }
      gauge.set(value);
      }
    });
    }

  const kit_uid = container.getAttribute('kit_id');

  const client = mqtt.connect('ws://test.mosquitto.org:8080', {
    clientId: 'javascript'
  });

  function publishMessage(message) {
    topic = kit_uid;
    console.log('Message published:', message);
    console.log('Publishing to topic:', topic);
    client.publish(topic, message, function(error) {
      if (error) {
        console.log('Publish error:', error);
      } else {
        console.log('Message successfully published');
      }
    });
  }

  client.on('connect', function() {
    client.subscribe(kit_uid);
    console.log(kit_uid+" connected");
  });



  client.on('message', function(topic, message) {
    console.log(message.toString());
    const widgetID = message.toString().split(':')[0];
    console.log(widgetID);
    value = message.toString().split(':')[1];
    target_divs = document.querySelectorAll(`div[widget__id="${widgetID}"]`);
    target_divs.forEach((target_div) => {
      const valueElement = target_div.querySelector('div[class="value"]');
      const chartElement = target_div.querySelector('.chart');
      const gaugeElement = target_div.querySelector('.gauge');
      const alertElement = target_div.querySelector('.lottie');

      if (valueElement) {
        valueElement.textContent = value;
      }
      if (chartElement) {
        const timestamp = new Date().toLocaleTimeString('en-US', {
          hour12: false
        });
        addDataToChart(widgetID, timestamp, value);
      }
      if (gaugeElement) {
        // Update the gauge value
        updateGaugeValue(widgetID, value);
      }
      if (alertElement) {
        if (value == "0"){
          alertElement.innerHTML = `<lottie-player src="https://lottie.host/5705f941-bd40-43a6-8757-a34aefe8fb89/0TCP7zrJNp.json"></lottie-player>`;
        }
        else if (value == "1"){
          alertElement.innerHTML = `<lottie-player src="https://lottie.host/edf4b7bd-d520-4f3f-8a73-a27a0614fedb/p9bGY9laDz.json" background="transparent" speed="1" loop autoplay></lottie-player>`;
          setTimeout(function () {
            value = "0";
        }, 5000);
        }
      }
    });
  });

  function initializeGauge(gaugeWidget) {
    const gaugeCanvas = gaugeWidget.querySelector('.gauge__canvas');
    console.log(gaugeCanvas);
    const widget__id = gaugeWidget.getAttribute('widget__id');
  

    if (!gaugeCanvas.gauge) {
      const gaugeCanvasWidth = 100; // Set the desired width for the gauge canvas
      const gaugeCanvasHeight = 100; // Set the desired height for the gauge canvas
      gaugeCanvas.width = gaugeCanvasWidth;
      gaugeCanvas.height = gaugeCanvasHeight;
      var opts = {
        // Customize Gauge.js options here as needed
        angle: 0.35,
        lineWidth: 0.1,
        radiusScale: 1,
        pointer: {
          length: 0.6,
          strokeWidth: 0.035,
          color: '#6d5dfc',
        },
        limitMax: false,
        limitMin: false,
        colorStart: '#6d5dfc',
        colorStop: '#5b0eeb',
        strokeColor: '#FFFF',
        generateGradient: true,
        highDpiSupport: true,
      };
  
      // Create the Gauge instance
      var gauge = new Donut(gaugeCanvas).setOptions(opts);
      gauge.maxValue = 100;
      gauge.setMinValue(0);
      gauge.animationSpeed = 32;
      gauge.set(0);
  
      // Store the Gauge instance in the gaugeCanvas element for later use
      gaugeCanvas.gauge = gauge;
    } 
    gaugeData[widget__id] = gauge;
  }
  
});


