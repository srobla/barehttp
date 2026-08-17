const DEF_COLOR = "white";

let nRows = 8;
let color = "black"
let selectedColor = "black"
let mode = "Draw"
let drawing = false

const container = document.querySelector(".container");
const inputSize = document.querySelector("#gridSizeInput")
let modeButton = document.querySelector("#modeButton")
modeButton.onclick = toggleMode;
document.querySelector("#gridButton").onclick = toggleGrid;
document.querySelector("#cleanButton").onclick = cleanGrid;

function createGrid(n){
    let width = 100/n;
    const cell = document.createElement("div");
    cell.classList.toggle("cell")
    cell.classList.toggle("grid")
    cell.style.flexBasis = width + "%";
    
    for(let i = 0; i<n*n; i++){
        container.appendChild(cell.cloneNode(true));
    }

    /* Create event listeners fot all childs */
    container.childNodes.forEach((cell) => { cell.addEventListener("mouseover", changeColor);});
    container.childNodes.forEach((cell) => { cell.addEventListener("click", changeColor);});
    container.childNodes.forEach((cell) => { cell.addEventListener("click", toggleDrawing);});
}

function changeColor(e){
    if(e.type == "mouseover" && !drawing) return
    e.target.style.backgroundColor = color;
}

function toggleGrid(){
    Array.from(container.children).forEach((cell) => ( cell.classList.toggle("grid")));
}

function cleanGrid(){
    Array.from(container.children).forEach((cell) => ( cell.style.backgroundColor = DEF_COLOR));
}

function toggleMode(){
    if(mode == "Draw"){
        color = DEF_COLOR
        modeButton.textContent = mode
        mode = "Erase"
        return
    } 
    color = selectedColor
    modeButton.textContent = mode
    mode = "Draw"
}

function toggleDrawing(){
    drawing = !drawing
}

function deleteGrid(){
    Array.from(container.children).forEach((cell) => ( cell.remove() ))
}

function changeGrid(e){
    if(0 > e.target.value || e.target.value > 100) return
    console.log(e.target.value)
    deleteGrid()
    createGrid(e.target.value)
}

createGrid(nRows)

inputSize.addEventListener("input", changeGrid)