let ctx = document.getElementById('temperatureChart').getContext('2d');

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


let socket;

function connectWebSocket() {
    socket = new WebSocket(`ws://${window.location.hostname}/ws`);

    socket.onopen = function (event) {
        console.log('WebSocket connection established');
        // socket.send();
    };

    socket.onmessage = function (event) {
        let temperature = parseFloat(event.data);
        let time = new Date();

        // pushes data and time to the chart
        temperatureChart.data.datasets[0].data.push(temperature);
        temperatureChart.data.labels.push(time);

        // updates the chart to show new input
        temperatureChart.update();

        // sends new temperature to updateTemperature
        updateTemperature(temperature);
    };

    socket.onclose = function (event) {
        console.log('WebSocket connection closed');
        // Try reconnecting after a delay
        setTimeout(connectWebSocket, 60000);
    };

    socket.onerror = function (error) {
        console.error('WebSocket error: ', error);
        // Close and reopen the WebSocket connection
        socket.close();
    };
}

// updates html element to output given temperature
function updateTemperature(temperature) {
    document.getElementById('temperature').innerText = "Current temperature: " + temperature + "°C";
}

window.onload = function () {
    connectWebSocket();
};

