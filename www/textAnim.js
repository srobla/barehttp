let str1 = "CS student.";
var str2 = "math student.";
var str3 = "web dev.";
var str4 = "freelancer.";
var startText = "Im a "
var arrOfStr = [str1, str2, str3, str4];
var arrOfArrs = arrOfStr.map( str => str.split(""));
var actualArr = 0;
var i = 1;
const text = document.getElementById("myDesc");
var addTiming = 100;
var timingToRead = 1500;
var subTiming = 50;




function addLetter(){
    setTimeout(function() {   
        text.innerHTML = startText + arrOfArrs[actualArr].slice(0, i).join("");
        i++;                  
        if (i < arrOfArrs[actualArr].length + 1) {           
          addLetter();             
        } 
        else{
          setTimeout(subLetter, timingToRead);
        }                      
      }, addTiming)
}

function subLetter(){
  setTimeout(function() {   
    text.innerHTML = startText + arrOfArrs[actualArr].slice(0, i).join("");
    i--;                  
    if (i > 0) {           
      subLetter();             
    } 
    else{
      if(actualArr < arrOfArrs.length-1){
        actualArr++;
      }else{
        actualArr = 0;
      }
      
      setTimeout(addLetter, addTiming);
    }                      
  }, subTiming)
}

setTimeout(addLetter, 3500);
