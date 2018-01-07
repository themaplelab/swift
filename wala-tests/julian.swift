func use_ints(num1: Int, num2: Int, num3: Int) -> Int{
   var num4 = num1 * num2
   if (num1 < 5 ) {
    return (num1 + num2 - num3 * num4)
   } else {
       return num4
   }
}

var t = use_ints(num1: 1, num2: 2, num3: 3); 
