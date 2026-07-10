function F() { console.log(new.target === F); } new F(); F();
