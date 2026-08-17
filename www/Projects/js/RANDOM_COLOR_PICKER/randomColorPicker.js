const container = document.getElementById("container")
const template = document.querySelector("template")
const addTileButton = document.getElementById("addtile")

function addTile(){
    const clone = template.content.cloneNode(true);
    const tile = clone.getElementById("tile");
    container.prepend(tile);
    console.log(container.childElementCount)
    if(container.childElementCount == 6){
        document.getElementById("addtile").style.display = "none";
    }
}

function randomColor(button){
    let tile = button.parentElement;
    let alph = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, "A", "B", "C", "D", "E", "F"]
    let color = "#"
    for(let i = 0; i<6; i++){
        let num = Math.floor(Math.random()*16)
        color += alph[num];
    }
    tile.style.backgroundColor = color;
    tile.firstElementChild.innerHTML = color;
    
     
}
