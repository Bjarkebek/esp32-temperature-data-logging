/**
 * @file Main script for handling temperature data and updating the chart.
 */

/**
 * @var {CanvasRenderingContext2D} ctx - Context for the temperature chart canvas.
 */
let ctx = document.getElementById('temperatureChart').getContext('2d');

/**
 * @var {Chart} temperatureChart - Chart object for displaying temperature data.
 */
let temperatureChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [{
            label: 'Temperature (°C)',
            data: [],
            backgroundColor: 'rgba(255, 99, 132, 0.2)',
            borderColor: 'rgba(255, 99, 132, 1)',
            borderWidth: 1
        }]
    },
    options: {
        scales: {
            x: {
                type: 'time',
                time: {
                    unit: 'minute'
                }
            },
            y: {
                beginAtZero: false
            }
        }
    }
});

/**
 * @var {WebSocket} socket - WebSocket object for connecting to the server.
 */

let socket;

/**
 * Establishes a WebSocket connection to the server.
 */
function connectWebSocket() {
    socket = new WebSocket(`ws://${window.location.hostname}/ws`);

    socket.onopen = function (event) {
        console.log('WebSocket connection established');
    }

    socket.onmessage = function (event) {
        let temperature = parseFloat(event.data);
        let time = new Date();

        temperatureChart.data.datasets[0].data.push(temperature);
        temperatureChart.data.labels.push(time);

        /**
         * Limit the number of data points displayed to keep the chart readable (24 hours)
         */
        let maxDataPoints = 144;
        if (temperatureChart.data.labels.length >= maxDataPoints) {
            temperatureChart.data.labels.shift();
            temperatureChart.data.datasets[0].data.shift();
        }

        /**
         * Update the chart
         */
        temperatureChart.update();

        /**
         * Send new temperature to updateTemperature
         */
        updateTemperature(temperature);
    }

    socket.onclose = function (event) {
        console.log('WebSocket connection closed');
        /**
         * Try reconnecting after a delay
         */
        setTimeout(connectWebSocket, 5000);
    }

    socket.onerror = function (error) {
        console.error('WebSocket error: ', error);
        /**
         * Close and reopen the WebSocket connection
         */
        socket.close();
    }
}

/**
 * Updates the HTML element to display the current temperature.
 * @param {number} temperature - The current temperature value.
 */
function updateTemperature(temperature) {
    document.getElementById('temperature').innerText = "Current temperature: " + temperature + "°C";
}

/**
 * Establish WebSocket connection when the window loads
 */
window.onload = function () {
    connectWebSocket();
}