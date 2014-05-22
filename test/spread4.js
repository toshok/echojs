// testing closing over rest parameters

function double(...things) {
   return function() {
     return things.map( (x) => 2 * x ); 
   };
}

var doubler = double(1, 2, 3, 4);
console.log(doubler());
