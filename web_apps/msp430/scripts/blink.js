let countdownInterval;
let ledElement;
let countdownElement;
let countdownValueElement;
let stopButton;

// Function to start the LED blink animation
function startLEDBlink() {
    ledElement = document.getElementById('led');
    countdownElement = document.getElementById('countdown');
    countdownValueElement = document.getElementById('countdownValue');
    stopButton = document.getElementById('stopButton');
    let countdownValue = 5;  // Countdown from 5 seconds

    // Show the countdown element
    countdownElement.style.display = 'block';
    countdownValueElement.textContent = countdownValue;

    // Update the countdown every second
    countdownInterval = setInterval(() => {
        countdownValue -= 1;
        countdownValueElement.textContent = countdownValue;

        // When countdown reaches 0, start the LED blink animation
        if (countdownValue === 0) {
            clearInterval(countdownInterval);
            countdownElement.style.display = 'none';
            ledElement.style.display = 'block';
            ledElement.style.animation = 'blink 1s infinite';
            stopButton.style.display = 'block';
        }
    }, 1000);
}

// Function to stop the LED blink animation
function stopLEDBlink() {
    clearInterval(countdownInterval);
    ledElement.style.animation = 'none';
    ledElement.style.display = 'none';
    stopButton.style.display = 'none';
    countdownElement.style.display = 'none';
}

// Add event listener to the start button
document.getElementById('startButton').addEventListener('click', () => {
    const userConfirmed = confirm('Are you sure you want to start the LED blink simulation?');
    if (userConfirmed) {
        startLEDBlink();
    }
});

// Add event listener to the stop button
document.getElementById('stopButton').addEventListener('click', stopLEDBlink);
