// server.js
const express = require('express');
const fs = require('fs');
const app = express();
const port = 3000;

app.use(express.static('public'));

app.get('/data', (req, res) => {
  fs.readFile('gas_levels.txt', 'utf8', (err, data) => {
    if (err) {
      res.status(500).send('Error reading data file');
      return;
    }

    // Split the file content into lines
    const lines = data.trim().split('\n');

    if (lines.length === 0) {
      res.status(404).send('No data available');
      return;
    }

    // Get the last line
    const lastLine = lines[lines.length - 1];

    // Parse the last line into JSON
    const [timestamp, gasLevel] = lastLine.split(',').map(item => item.trim());
    const result = { timestamp: parseInt(timestamp), gasLevel: parseInt(gasLevel) };

    res.json(result);
  });
});

app.listen(port, '0.0.0.0', () => {
  console.log(`Server running at http://localhost:${port}`);
});
