
const playerMove = ["Rock", "Paper", "Scissors"]
const AIMove = ["Rock", "Paper", "Scissors"]
const puntuationDisplay = document.getElementById("displayCount");
const playDisplay = document.getElementById("displayPlay");
const winnerDisplay = document.getElementById("whoWin");
const endScreen = document.getElementById("endScreen");
const endScreenText = document.getElementById("endScreenText");
var points = [0, 0]

function play(m){
    let im = AIMove[Math.floor(Math.random()*3)];
    let pm = playerMove[m]
    playDisplay.innerHTML = "Your Move: " + pm + " - AI Move: " + im
    if((pm == "Rock" && im == "Scissors")||(pm == "Scissors" && im == "Paper")||(pm == "Paper" && im == "Rock")){
        points[0] += 1;
        winnerDisplay.innerHTML = "You win"
    }else if((im == "Rock" && pm == "Scissors")||(im == "Scissors" && pm == "Paper")||(im == "Paper" && pm == "Rock")){
        points[1] += 1;
        winnerDisplay.innerHTML = "AI wins"

    }else{
        winnerDisplay.innerHTML = "Draw"

    }
    playDisplay.innerHTML = "Your Move: " + pm + " - AI Move: " + im
   puntuationDisplay.innerHTML = "Player: " + points[0] + " AI: " + points[1]

   //end play
   if(points[0] == 10){
    endScreen.classList.remove("invisible")
    endScreenText.innerHTML = "You Win"

   }
   else if(points[1] == 10){
    endScreen.classList.remove("invisible")
    endScreenText.innerHTML = "AI Wins"

    //AI wins
   }
}

function reload(){
    location.reload();
}
