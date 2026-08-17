console.log("NEW LOAD")

const choicesInput = document.getElementById("choicesInput");
const choicesDisplay = document.getElementById("choicesDisplay");
const choiceOutput = document.getElementById("choice");

function rnd(min, max){
    return Math.floor(Math.random()* (max-min) +min)
}

function pickRandom(){
    let choicesStr = choicesInput.value;
    let choicesList = choicesStr.split(",");
    console.log(choicesList);
    choicesDisplay.innerHTML = "Options: "  + choicesList.join(" / ")
    choiceOutput.innerHTML = "- " + choicesList[rnd(0, choicesList.length)].toUpperCase() + " -"
}