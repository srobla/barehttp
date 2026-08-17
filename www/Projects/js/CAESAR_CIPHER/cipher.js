let decode_mode=false;

function updateValue(){
    let rot = document.getElementById("num").value;
    console.log("funciona ")
    if(decode_mode){
        document.getElementById("mode").innerHTML = "DECODE ROT" + rot;
    }else{
        document.getElementById("mode").innerHTML = "ENCODE ROT" +  rot;
    }
}

function rot(str, num) {
    num = parseInt(num);
    let result="";
    let arr = str.toUpperCase().split("");
    let alph = "ABCDEFGHIJKLMNOPQRSTUVWXYZ".split("");
    if(decode_mode){
        console.log("decode")
        function decodeLetter(letter){
            let index = alph.indexOf(letter);
            let newIndex = (index >= num)? index-num: 26-(num-index);
            let newLetter = alph[newIndex];
            return newLetter;
          }
          result = arr.map(letter=>( /[A-Z]/.test(letter)? decodeLetter(letter): letter));
          console.log(result.join(""));
    }else{
        console.log("encode")
        function encodeLetter(letter){
            let index = alph.indexOf(letter);
            let newIndex = (index + num >= 26)? num + index - 26: num + index;
            let newLetter = alph[newIndex];
            return newLetter;
          }
          result = arr.map(letter=>(/[A-Z]/.test(letter)? encodeLetter(letter): letter));
        }
    console.log(result.join(""));
    return result.join("");
  }

function changeMode(){
    if(decode_mode){
        decode_mode = false;
        updateValue();
        document.getElementById("cipherButton").innerHTML = "ENCODE";
    }else{
        decode_mode = true;
        updateValue();
        document.getElementById("cipherButton").innerHTML = "DECODE";
    }
}

function cipher(){
    let input = document.getElementById("input").value;
    console.log(input);
    let num  = document.getElementById("num").value;
    document.getElementById("output").value = rot(input, num);
}


//   "SERR PBQR PNZC"