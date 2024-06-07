// Function to toggle LED state
function toggleLED(led) {
    led.classList.toggle('on');
}

// Add event listeners to each LED element
document.getElementById('led1').addEventListener('click', () => {
    toggleLED(document.getElementById('led1'));
});

document.getElementById('led2').addEventListener('click', () => {
    toggleLED(document.getElementById('led2'));
});

document.getElementById('led3').addEventListener('click', () => {
    toggleLED(document.getElementById('led3'));
});