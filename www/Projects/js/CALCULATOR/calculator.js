const MAX_OUTPUT = 16

const numberButtons = document.querySelectorAll("button.number")
const operatorButtons = document.querySelectorAll("button.operator")
const cleanButton = document.querySelector("#clean")
const invertButton = document.querySelector("#invert")
const equalButton = document.querySelector("#equal")
const deleteButton = document.querySelector("#delete")
const pointButton = document.querySelector("#point")
const outputText = document.querySelector("#outputText")
const history = document.querySelector("#history")

numberButtons.forEach((button) => button.addEventListener("click", function(e){
   writeOperand(e.target.innerText)
}))
operatorButtons.forEach((button) => button.addEventListener("click", function(e){
    setOperator(e.target.innerText)
}))
cleanButton.addEventListener("click", clean)
invertButton.addEventListener("click", invert)
equalButton.addEventListener("click", operate)
pointButton.addEventListener("click", addDecimal)
deleteButton.addEventListener("click", del)

/* Specific functions for each click listener. 
    Specific variables for: each operand 
*/
let op1 = 0
let op2 = 0
let operator = ""
let mode = "o1" /* o1, o1d, o2, o2d */
let lastOp = "0"

function print(str){
    outputText.textContent = str
    if(mode == "o1"){
        history.textContent = lastOp
    }else{
        history.textContent = op1
    }
}

function length(n){
    return `${n}`.length
}

function writeOperand(n){
    if(mode == "o1") {
        if(op1.length >= 16) return;
        op1 += n
        op1 = asNumber(op1)
        print(op1)
    }

    else if(mode == "o2") {
        if(op2.length >= 16) return;
        op2 += n
        op2 = asNumber(op2)
        print(op2)
    }
}

function asNumber(string){
    return Number(string).toString()
}

function setOperator(char){
    if(mode == "o2") operate()
    operator = char
    mode = "o2"
    print(op2)
}

function operate(){
    let x = 0
    op1 = parseFloat(op1)
    op2 = parseFloat(op2)
    switch (operator) {
    case "+":
        x = op1 + op2
        break;
    case "-":
        x = op1 - op2
        break;
    case "x":
        x = op1 * op2
        break;

    case "/":
        x = op1 / op2
        break
    default:
        break;
    }
    lastOp = op1 + " " + operator + " " + op2 + " ="
    op1 = asNumber(x.toFixed(4))
    operator = ""
    op2 = "0"
    mode = "o1"
    print(op1)
}

function invert(){
    if(mode == "o1"){
        if(op1[0] == "-") {
            op1 = op1.substring(1)
        }else{
            op1 = "-" + op1
        }
        print(op1)
    }else if(mode == "o2"){
        if(op2[0] == "-"){
            op2 = op2.substring(1)
        }else{
            op2 = "-" + op2
        }
        print(op2)
    } 
}

function clean(){
    op1 = op2 = lastOp = "0"
    print(op1)
}

function del(){
    if(mode == "o1"){
        op1 = op1.slice(0, -1)
        print(op1)
    }else if(mode == "o2"){
        op2 = op2.slice(0, -1)
        print(op2)
    }
}

function addDecimal(){
    if(mode == "o1"){
        if(op1 % 1 != 0) return;
        op1 += "."
        print(op1)
    }else if (mode == "o2"){
        if(op2 % 1 != 0) return;
        op2 += "."
        print(op2)
    }
}