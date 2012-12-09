console.log ("before while");
while (true) {
  try {
    console.log ("inside try");
    break;
  }
  finally {
    console.log ("out of try");
  }
}
console.log ("out of while");
