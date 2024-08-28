// script.js
function updateDateTime() {
    const now = new Date();
    const dateTimeString = now.toLocaleString();
    document.getElementById('datetime').textContent = dateTimeString;
}

function updateUI(data) {
    const dataDiv = document.getElementById('data');
    const gasLevel = data.gasLevel;
    let statusClass, statusText;

    if (gasLevel === 0) {
        statusClass = 'status-zero';
        statusText = 'Warning: Sensor may be faulty';
    } else if (gasLevel > 200) {
        statusClass = 'status-danger';
        statusText = 'Danger: High gas level';
    } else {
        statusClass = 'status-normal';
        statusText = 'Normal';
    }

    dataDiv.innerHTML = `
        <div class="gas-level ${statusClass}">
            <span class="value">${gasLevel}</span>
            <span class="unit">ppm</span>
        </div>
        <p class="status ${statusClass}">${statusText}</p>
    `;

    // Update body class for background effect
    document.body.className = statusClass;
}

function showError(message) {
    const dataDiv = document.getElementById('data');
    dataDiv.innerHTML = `<p class="error">${message}</p>`;
    document.body.className = '';
}

function fetchData() {
    fetch('/data')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.json();
        })
        .then(data => {
            if (data.error) {
                showError(`Error: ${data.error}`);
            } else {
                updateUI(data);
            }
        })
        .catch(error => {
            console.error('Fetch error:', error);
            showError('Failed to load data');
        });
}

// Update date and time every second
setInterval(updateDateTime, 1000);

// Fetch data every 1 second
setInterval(fetchData, 1000);

// Initial calls
updateDateTime();
fetchData();