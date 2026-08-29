// ===============================
// SMART HOME SCRIPT
// ===============================

let fanStatus = false;
let lightStatus = false;

// -------------------------------
// AI Voice Response
// -------------------------------

function speak(text){

    const speech = new SpeechSynthesisUtterance(text);

    speech.lang = "en-IN";

    speech.rate = 0.9;

    speech.pitch = 1;

    speech.volume = 1.0;

    window.speechSynthesis.cancel();

    window.speechSynthesis.speak(speech);

}

// -------------------------------
// Toast Notification
// -------------------------------

function showToast(message){

    let toast = document.createElement("div");

    toast.className = "toast";

    toast.innerHTML = message;

    document.body.appendChild(toast);

    setTimeout(function(){

        toast.classList.add("show");

    },100);

    setTimeout(function(){

        toast.classList.remove("show");

        setTimeout(function(){

            toast.remove();

        },400);

    },2500);

}

// -------------------------------
// Activity Log
// -------------------------------

function addActivity(text){

    let list = document.getElementById("activity-list");

    let item = document.createElement("li");

    let time = new Date().toLocaleTimeString();

    item.innerHTML = time + " - " + text;

    list.prepend(item);

}
// -------------------------------
// FAN CONTROL
// -------------------------------

async function toggleDevice(device, button){

    try{

        let url = fanStatus ? "/fan/off" : "/fan/on";

        let response = await fetch(url);

        let data = await response.json();

        if(!response.ok || !data.success){

            showToast("❌ " + (data.message || "Request failed"));

            return;

        }

        // Command is queued, not executed yet -- the ESP32 picks it up on
        // its next poll. checkESP32() will correct the UI once the device
        // actually confirms the new state; this just gives instant feedback.

        let targetOn = !fanStatus;

        let state = document.getElementById("fan-state");

        let icon = document.querySelector(".fa-fan");

        state.innerHTML = "Queued...";

        if(targetOn){

            speak("Turning on fan");

            showToast("🌀 Fan ON queued");

            addActivity("Fan ON queued");

        }else{

            speak("Turning off fan");

            showToast("🛑 Fan OFF queued");

            addActivity("Fan OFF queued");

        }

    }

    catch{

        showToast("❌ ESP32 Offline");

    }

}

// -------------------------------
// LIGHT CONTROL
// -------------------------------

async function toggleLight(button){

    try{

        let url = lightStatus ? "/light/off" : "/light/on";

        let response = await fetch(url);

        let data = await response.json();

        if(!response.ok || !data.success){

            showToast("❌ " + (data.message || "Request failed"));

            return;

        }

        let targetOn = !lightStatus;

        let state = document.getElementById("light-state");

        state.innerHTML = "Queued...";

        if(targetOn){

            speak("Turning on light");

            showToast("💡 Light ON queued");

            addActivity("Light ON queued");

        }else{

            speak("Turning off light");

            showToast("💡 Light OFF queued");

            addActivity("Light OFF queued");

        }

    }

    catch{

        showToast("❌ ESP32 Offline");

    }

}
// -------------------------------
// VOICE CONTROL
// -------------------------------

function startVoiceControl(){

    const SpeechRecognition =
        window.SpeechRecognition ||
        window.webkitSpeechRecognition;

    if(!SpeechRecognition){

        showToast("Voice recognition not supported");

        speak("Voice recognition is not supported.");

        return;

    }

    const recognition = new SpeechRecognition();

    recognition.lang = "en-IN";

    recognition.interimResults = false;

    recognition.continuous = false;

    recognition.maxAlternatives = 3;

    document.getElementById("voice-status").innerHTML =
        "🎤 Listening...";

    recognition.start();

    recognition.onresult = function(event){

        const command =
            event.results[0][0].transcript.toLowerCase().trim();

        document.getElementById("voice-status").innerHTML =
            "Command : " + command;

        // FAN ON

        if(
            command.includes("fan on") ||
            command.includes("turn on fan") ||
            command.includes("switch on fan")
        ){

            if(!fanStatus){

                toggleDevice(
                    "fan",
                    document.querySelectorAll(".toggle-btn")[0]
                );

            }

        }

        // FAN OFF

        else if(
            command.includes("fan off") ||
            command.includes("turn off fan") ||
            command.includes("switch off fan")
        ){

            if(fanStatus){

                toggleDevice(
                    "fan",
                    document.querySelectorAll(".toggle-btn")[0]
                );

            }

        }

        // LIGHT ON

        else if(
            command.includes("light on") ||
            command.includes("turn on light") ||
            command.includes("switch on light")
        ){

            if(!lightStatus){

                toggleLight(
                    document.querySelectorAll(".toggle-btn")[1]
                );

            }

        }

        // LIGHT OFF

        else if(
            command.includes("light off") ||
            command.includes("turn off light") ||
            command.includes("switch off light")
        ){

            if(lightStatus){

                toggleLight(
                    document.querySelectorAll(".toggle-btn")[1]
                );

            }

        }

        else{

            speak("Sorry, I did not understand.");

            showToast("Unknown Command");

        }

    };

    recognition.onerror = function(){

        showToast("Voice Recognition Error");

        speak("Voice recognition error.");

    };

    recognition.onend = function(){

        document.getElementById("voice-status").innerHTML =
            "Tap microphone and speak";

    };

}
// -------------------------------
// ESP32 STATUS CHECK
// -------------------------------

function applyFanState(isOn){

    fanStatus = isOn;

    let status = document.getElementById("status-fan");
    let state = document.getElementById("fan-state");
    let icon = document.querySelector(".fa-fan");
    let button = document.querySelectorAll(".toggle-btn")[0];

    status.innerHTML = isOn ? "ON" : "OFF";
    status.className = isOn ? "pill on" : "pill off";
    state.innerHTML = isOn ? "Running" : "Stopped";
    if(button) button.innerHTML = isOn ? "Turn OFF" : "Turn ON";
    if(icon) icon.classList.toggle("rotate", isOn);

}

function applyLightState(isOn){

    lightStatus = isOn;

    let status = document.getElementById("status-light");
    let state = document.getElementById("light-state");
    let button = document.querySelectorAll(".toggle-btn")[1];

    status.innerHTML = isOn ? "ON" : "OFF";
    status.className = isOn ? "pill on" : "pill off";
    state.innerHTML = isOn ? "ON" : "OFF";
    if(button) button.innerHTML = isOn ? "Turn OFF" : "Turn ON";

}

async function checkESP32(){

    try{

        const response = await fetch("/esp32/status");

        const data = await response.json();

        if(data.online){

            document.getElementById("esp32-status").innerHTML =
                "🟢 ESP32 Online";

            document.getElementById("esp-online").innerHTML =
                "Connected";

            // Source of truth: what the device itself last reported,
            // once a queued command has actually been executed.
            applyFanState(data.fan === "ON");
            applyLightState(data.light === "ON");

        }

        else{

            document.getElementById("esp32-status").innerHTML =
                "🔴 ESP32 Offline";

            document.getElementById("esp-online").innerHTML =
                "Disconnected";

        }

    }

    catch(error){

        document.getElementById("esp32-status").innerHTML =
            "🔴 ESP32 Offline";

        document.getElementById("esp-online").innerHTML =
            "Disconnected";

    }

}

// -------------------------------
// AUTO REFRESH
// -------------------------------

setInterval(function(){

    checkESP32();

},1000);


// -------------------------------
// PAGE LOAD
// -------------------------------

window.onload = function(){

    checkESP32();

    addActivity("Dashboard Loaded");

    showToast("Welcome " + new Date().toLocaleTimeString());

    speak("Welcome to Smart Home");

};