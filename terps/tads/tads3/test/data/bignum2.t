#include "tads.h"
#include "t3.h"
#include "dict.h"
#include "bignum.h"

main(args)
{
    local x, y, z, lst;

    /*
     *   creation tests 
     */
    x = 3.1415927;
    y = 1234567890987654321.;
    y = 12345678909876543210.;
    y = new BigNumber(100, 10);
    y = new BigNumber(12345, 32);
    y = new BigNumber(123456, 33);

    /*
     *   display tests 
     */
    tadsSay(1234.5678); "\n";
    tadsSay(x); "\n";
    tadsSay(y); "\n";
    tadsSay(123456789234567893456789456789567896789789899.); "\n";
    tadsSay(100e-2000); "\n";
    tadsSay(5.9999988888e5000); "\n";
    tadsSay(12345e-20000); "\n";
    tadsSay(1234e-8); "\n";
    tadsSay(.090807); "\n";
    "\b";

    /*
     *   string formatting tests 
     */
    
    x = toString(.75e4);
    tadsSay(x); "\n";
    "\b";

    x = 1234.5362;
    "(4): <<x.formatString(4)>>\n";
    "(5): <<x.formatString(5)>>\n";
    "(6): <<x.formatString(6)>>\n";
    "(7): <<x.formatString(7)>>\n";
    "\b";
    
    x = 999.99999;
    "(8): <<x.formatString(8)>>\n";
    "(7): <<x.formatString(7)>>\n";
    "(5): <<x.formatString(5)>>\n";
    "(4): <<x.formatString(4)>>\n";
    "(3): <<x.formatString(3)>>\n";
    "(2): <<x.formatString(2)>>\n";
    "(7,0,0,3): <<x.formatString(7, 0, 0, 3)>>\n";
    "(7,0,0,5): <<x.formatString(7, 0, 0, 5)>>\n";
    "(8,EXP): <<x.formatString(8, BignumExp)>>\n";
    "(7,EXP): <<x.formatString(7, BignumExp)>>\n";
    "(3,EXP+PT): <<x.formatString(3, BignumExp | BignumPoint)>>\n";
    "(8,EXP,0,2): <<x.formatString(8, BignumExp, 0, 2)>>\n";
    "(10,0,7,5): <<x.formatString(10, 0, 7, 5)>>\n";
    "(10,0,7,3): <<x.formatString(10, 0, 7, 3)>>\n";

    "(15,0,12,3,0,'*'): <<x.formatString(15, 0, 12, 3, 0, '*')>>\n";
    "(15,0,12,3,0,'/*\\*'): <<x.formatString(15, 0, 12, 3, 0, '/*\\*')>>\n";
    "(15,COMMAS,12,3,0,'/*\\*'):
        <<x.formatString(15, BignumCommas, 12, 3, 0, '/*\\*')>>\n";

    x = 12345.6789;
    "(15,COMMAS,12,3,0,'/*\\*'):
        <<x.formatString(15, BignumCommas, 12, 3, 0, '/*\\*')>>\n";

    x = 1234567.8900;
    "(15,COMMAS,12,3,0,'/*\\*'):
        <<x.formatString(15, BignumCommas, 12, 3, 0, '/*\\*')>>\n";

    x = 12345678.8900;
    "(15,COMMAS,12,3,0,'/*\\*'):
        <<x.formatString(15, BignumCommas, 12, 3, 0, '/*\\*')>>\n";

    x = 12345678.98765;
    "(20,COMMAS): <<x.formatString(20, BignumCommas)>>\n";
    "(20,COMMAS+EURO):
        <<x.formatString(20, BignumCommas | BignumEuroStyle)>>\n";
    "(20 - no space):<<x.formatString(20)>>\n";
    "(20,POS_SPACE):<<x.formatString(20, BignumPosSpace)>>\n";

    x = 123456789.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";

    x = .00098765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    x = 0.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    x = 1.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    x = 12.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    x = 123.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    x = 1234.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    x = 12345.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    x = 123456.98765;
    "(20,COMMAS):<<x.formatString(20, BignumCommas)>>\n";
    "\b";

    /*
     *   Zero formatting tests 
     */

    x = 0.;
    "zero: <<x>>\n";
    "(3,EXP): <<x.formatString(3, BignumExp)>>\n";
    "(8,EXP+PT): <<x.formatString(8, BignumExp | BignumPoint)>>\n";
    "(8,EXP,0,2): <<x.formatString(8, BignumExp, 0, 2)>>\n";
    "\b";

    /*
     *   equality comparison tests 
     */
    
    x = 123.456;
    y = 123.45600001;
    "x = <<x>>, y = <<y>>\n";
    "(equal exact): <<x == y ? 'yes' : 'no'>>\n";
    "(equal with rounding): <<x.equalRound(y) ? 'yes' : 'no'>>\n";

    x = 123.456;
    y = 1.23456e2;
    "x = <<x>>, y = <<y>>\n";
    "(equal exact): <<x == y ? 'yes' : 'no'>>\n";
    "(equal with rounding): <<x.equalRound(y) ? 'yes' : 'no'>>\n";

    x = 100.;
    y = 99.999;
    "x = <<x>>, y = <<y>>\n";
    "(equal exact): <<x == y ? 'yes' : 'no'>>\n";
    "(equal with rounding): <<x.equalRound(y) ? 'yes' : 'no'>>\n";

    x = 100.001;
    y = 100.002;
    "x = <<x>>, y = <<y>>\n";
    "(equal exact): <<x == y ? 'yes' : 'no'>>\n";
    "(equal with rounding): <<x.equalRound(y) ? 'yes' : 'no'>>\n";
    "\b";

    /*
     *   Magnitude comparison tests 
     */

    x = 123.456;
    y = 123.4561;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = 123.456;
    y = -123.4561;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = -123.4561;
    y = 123.4560;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = -123.456;
    y = -123.4561;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = 999.;
    y = .999;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = -999.;
    y = -.999;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = 1.;
    y = 2.;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = 111.01;
    y = 111.10;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = 0.0000;
    y = 35.3;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    x = -32.000;
    y = 0.000;
    "x = <<x>>, y = <<y>>, x &lt; y = <<bool(x<y)>>, x > y = <<bool(x>y)>>\n";

    "\b";

    /*
     *   Get/Set-Precision tests 
     */

    x = 1234567890.;
    "x = <<x>>, x.getPrecision() = <<x.getPrecision()>>,
    x.setPrecision(5) = <<x.setPrecision(5)>>, ";

    "x.setPrecision(3) = <<x.setPrecision(3)>>\n";
    
    "x.setPrecision(15) = <<x.setPrecision(15)>>\n";
    "\b";

    /*
     *   Addition and subtraction tests 
     */

    x = 12345.6789;
    y = 3.14159265;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 1.999999;
    y = 2.001;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 1200.;
    y = .0533;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 1200.;
    y = .5335;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 9999.;
    y = .5335;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x+y+1 = <<x+y+1>>\n";
    "x-y = <<x-y>>\n";

    x = 9999.;
    y = .4999;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x+y+1 = <<x+y+1>>\n";
    "x-y = <<x-y>>\n";

    x = 9999.;
    y = 1.999;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 9999.;
    y = 9.999;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 9999.;
    y = 5.999;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 9999.;
    y = 6.999;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 9999.;
    y = 3.999;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 1234.;
    y = -5678.;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = -1234.;
    y = 5678.;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    x = 1000.1;
    y = .0001;
    "x = <<x>>, y = <<y>>, x+y = <<x+y>>, x-y = <<x-y>>\n";

    "\b";

    /*
     *   Multiplication 
     */
    x = 1.;
    y = 1.00001;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 3456.;
    y = 3.;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";
    
    x = 1234.;
    y = 5678.;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 1.999;
    y = 9.99;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 3.14159265;
    y = 0.25;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 8751.;
    y = 1.1110000;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 8751.;
    y = 1.111;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 8751.;
    y = 9.0009;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 8751.;
    y = 9.000009;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = -3883.1;
    y = 57.6010199;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = 42.7;
    y = -177.;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    x = -.979;
    y = -3.203;
    "x = <<x>>, y = <<y>>, x*y = <<x*y>>\n";

    "\b";

    /*
     *   Division 
     */

    x = -.979;
    y = -3.203;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 32.;
    y = 522.;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 7205.;
    y = 3.;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 1.;
    y = 1.00001;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 3456.;
    y = 3.;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";
    
    x = 1234.;
    y = 5678.;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 1.999;
    y = 9.99;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 3.14159265;
    y = 0.25;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 8751.;
    y = 1.1110000;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 8751.;
    y = 1.111;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 8751.;
    y = 9.0009;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 8751.;
    y = 9.000009;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = -3883.1;
    y = 57.6010199;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = 42.7;
    y = -177.;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    x = -.979;
    y = -3.203;
    "x = <<x>>, y = <<y>>, x/y = <<x/y>>\n";

    /*
     *   Remainders
     */

    x = 12345.;
    y = 77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = 12397.;
    y = 77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = 12396.;
    y = 77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = 77.;
    y = 123.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = -12345.;
    y = 77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = 12345.;
    y = -77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = -12345.;
    y = -77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = -10.;
    y = -3.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = 123e5;
    y = 77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    x = 0.000;
    y = 77.;
    lst = x.divideBy(y);
    "x = <<x>>, y = <<y>>, x/y = <<lst[1]>>, x mod y = <<lst[2]>>\n";

    "\b";

    /*
     *   Fraction/Whole tests 
     */

    x = 1234.5678;
    "x = <<x>>, frac = <<x.getFraction()>>, whole = <<x.getWhole()>>\n";

    x = .9325773;
    "x = <<x>>, frac = <<x.getFraction()>>, whole = <<x.getWhole()>>\n";

    x = 8.710243;
    "x = <<x>>, frac = <<x.getFraction()>>, whole = <<x.getWhole()>>\n";

    x = 1.234e-15;
    "x = <<x>>, frac = <<x.getFraction()>>, whole = <<x.getWhole()>>\n";

    x = 1234567800000000.;
    "x = <<x>>, frac = <<x.getFraction()>>, whole = <<x.getWhole()>>\n";

    "\b";

    /*
     *   RoundToDecimal tests 
     */

    x = 1234.53739;
    "x = <<x>>, round(0) = <<x.roundToDecimal(0)>>\n
    ... round(1) = <<x.roundToDecimal(1)>>\n
    ... round(2) = <<x.roundToDecimal(2)>>\n
    ... round(3) = <<x.roundToDecimal(3)>>\n
    ... round(4) = <<x.roundToDecimal(4)>>\n
    ... round(5) = <<x.roundToDecimal(5)>>\n
    ... round(6) = <<x.roundToDecimal(6)>>\n
    ... round(7) = <<x.roundToDecimal(7)>>\n
    ... round(22) = <<x.roundToDecimal(22)>>\n
    ... round(-1) = <<x.roundToDecimal(-1)>>\n
    ... round(-2) = <<x.roundToDecimal(-2)>>\n
    ... round(-3) = <<x.roundToDecimal(-3)>>\n
    ... round(-4) = <<x.roundToDecimal(-4)>>\n
    ... round(-5) = <<x.roundToDecimal(-5)>>\n
    ... round(-10) = <<x.roundToDecimal(-10)>>\n";
    x = 9999.99999;
    "x = <<x>>, round(0) = <<x.roundToDecimal(0)>>\n
    ... round(1) = <<x.roundToDecimal(1)>>\n
    ... round(2) = <<x.roundToDecimal(2)>>\n
    ... round(3) = <<x.roundToDecimal(3)>>\n
    ... round(4) = <<x.roundToDecimal(4)>>\n
    ... round(5) = <<x.roundToDecimal(5)>>\n
    ... round(6) = <<x.roundToDecimal(6)>>\n
    ... round(7) = <<x.roundToDecimal(7)>>\n
    ... round(22) = <<x.roundToDecimal(22)>>\n
    ... round(-1) = <<x.roundToDecimal(-1)>>\n
    ... round(-2) = <<x.roundToDecimal(-2)>>\n
    ... round(-3) = <<x.roundToDecimal(-3)>>\n
    ... round(-4) = <<x.roundToDecimal(-4)>>\n
    ... round(-5) = <<x.roundToDecimal(-5)>>\n
    ... round(-10) = <<x.roundToDecimal(-10)>>\n";

    "\b";

    /*
     *   abs, floor, ceil tests 
     */
    x = 0.1234;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = -0.1234;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = 7.1234;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = -7.1234;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = 0.0000;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = 1.4e-5;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = -1.4e-5;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = 999.9999;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = -999.9999;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = 999.0000000001;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    x = -999.0000000001;
    "x = <<x>>, abs(x) = <<x.getAbs()>>, ceil(x) = <<x.getCeil()>>,
    floor(x) = <<x.getFloor()>>\n";

    "\b";

    /*
     *   scaleTen tets 
     */
    
    x = 12345.;
    for (local i = -5 ; i <= 5 ; ++i)
        "x = <<x>>, x.scaleTen(<<i>>) = <<x.scaleTen(i)>>\n";

    "x.getScale() = <<x.getScale()>>\n";
    for (local i = -5 ; i <= 5 ; ++i)
        "x.scaleTen(<<i>>).getScale() = <<x.scaleTen(i).getScale()>>\n";

    "\b";

    /*
     *   negate tests 
     */
    x = 0.;
    "x = <<x>>, x.negate() = <<x.negate()>>\n";

    x = 123.;
    "x = <<x>>, x.negate() = <<x.negate()>>\n";

    x = 0.123;
    "x = <<x>>, x.negate() = <<x.negate()>>\n";

    x = -0.123;
    "x = <<x>>, x.negate() = <<x.negate()>>\n";

    x = -987.;
    "x = <<x>>, x.negate() = <<x.negate()>>\n";

    x = -987.;
    "x = <<x>>, -x = <<-x>>\n";

    x = 123.456;
    "x = <<x>>, -x = <<-x>>\n";

    "\b";

    /*
     *   copySignFrom tests 
     */
    x = 123.;
    y = .345;
    "x = <<x>>, y = <<y>>, x.copySignFrom(y) = <<x.copySignFrom(y)>>\n";

    x = 123.;
    y = -.345;
    "x = <<x>>, y = <<y>>, x.copySignFrom(y) = <<x.copySignFrom(y)>>\n";

    x = -123.;
    y = .345;
    "x = <<x>>, y = <<y>>, x.copySignFrom(y) = <<x.copySignFrom(y)>>\n";

    x = -123.;
    y = -.345;
    "x = <<x>>, y = <<y>>, x.copySignFrom(y) = <<x.copySignFrom(y)>>\n";

    x = 0.;
    y = .345;
    "x = <<x>>, y = <<y>>, x.copySignFrom(y) = <<x.copySignFrom(y)>>\n";

    x = 0.;
    y = -3.345;
    "x = <<x>>, y = <<y>>, x.copySignFrom(y) = <<x.copySignFrom(y)>>\n";

    "\b";

    /*
     *   IsNegative tests 
     */

    x = 0.;
    "x = <<x>>, x.isNegative = <<x.isNegative() ? 'yes' : 'no'>>\n";

    x = 123.;
    "x = <<x>>, x.isNegative = <<x.isNegative() ? 'yes' : 'no'>>\n";

    x = -123.;
    "x = <<x>>, x.isNegative = <<x.isNegative() ? 'yes' : 'no'>>\n";

    x = 0.005;
    "x = <<x>>, x.isNegative = <<x.isNegative() ? 'yes' : 'no'>>\n";

    x = -0.005;
    "x = <<x>>, x.isNegative = <<x.isNegative() ? 'yes' : 'no'>>\n";

    "\b";

    /*
     *   toInteger tests 
     */

    x = 0.;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 123.;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 0.456;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 0.567;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 987.568;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 893.499;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 2147483646.299;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 2147483646.832;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 2147483647.299;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 2147483647.832;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = -2147483647.200;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = -2147483647.822;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = -2147483648.200;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = -2147483648.822;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 9999999999.200;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = -9999999999.200;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = 2149.7483647200;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    x = -2149.7483647200;
    try { "x = <<x>>, toInteger(x) = <<toInteger(x)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    "\b";

    /*
     *   Sine 
     */
    x = 0.785398164; // pi/4 -> .707106782
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 1.57079633; // pi/2 -> 1.0000000
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 3.14159265; // pi -> 0
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 1.11111; // -> 0.896192
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 2.0000; // -> 0.909297 -> .90930
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 3.010203; // -> .1310119
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 4.567890; // -> -.9895782
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 5.43210; // -> -.751996
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 6.012345; // -> -.2675412
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 6.305555; // a little over 2pi -> .02236783
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 9.350123; // -> 0.07458563
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -0.785398164; // -pi/4 -> -.707106782
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -1.57079633; // -pi/2 -> -1.0000000
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -3.14159265; // -pi -> 0
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -1.11111; // -> -0.896192
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -2.0000; // -> -0.909297 -> -.90930
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -3.010203; // -> -.1310119
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -4.567890; // -> .9895782
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -5.43210; // -> .751996
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -6.012345; // -> .2675412
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -6.305555; // a little over -2pi -> -.02236783
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = -9.350123; // -> -0.07458563
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 0.00000000; // -> .00000000
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 0.50000000; // -> .47942554
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 112.; // -> -0.88999560 -> -0.89
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    x = 112000.; // -> 0.79541568
    "x = <<x>>, sin(x) = <<x.sine()>>\n";

    "\b";

    /*
     *   Cosine 
     */
    x = 0.785398164; // pi/4 -> 0.707106781
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 1.57079633; // pi/2 -> 0
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 3.14159265; // pi -> -1
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 1.11111; // -> .443667
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 2.0000; // -> -0.41615
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 3.010203; // -> -.9913808
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 4.567890; // -> -.1439967
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 5.43210; // -> .659167
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 6.012345; // -> .9635464
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 6.305555; // a little over 2pi -> .9997498
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 9.350123; // -> -.9972146
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -0.785398164; // -pi/4 -> .707106781
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -1.57079633; // -pi/2 -> 0
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -3.14159265; // -pi -> -1
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -1.11111; // -> 0.443667
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -2.0000; // -> -.41615
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -3.010203; // -> -.9913808
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -4.567890; // -> -.1439967
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -5.43210; // -> .659167
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -6.012345; // -> .9635464
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -6.305555; // a little over -2pi -> .9997498
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = -9.350123; // -> -.9972146
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 0.00000000; // -> 1
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 0.50000000; // -> .87758256
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 112.; // -> .456
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    x = 112000.; // -> -.606064
    "x = <<x>>, cos(x) = <<x.cosine()>>\n";

    "\b";

    /*
     *   Tangent
     */
    x = 0.785398164; // pi/4 -> 1
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 1.57079633; // pi/2 -> -312012480.5 (really -INF)
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 3.14159265; // pi -> 0
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 1.11111; // -> 2.01996
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 2.0000; // -> -2.1850
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 3.010203; // -> -.1321510
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 4.567890; // -> 6.872231
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 5.43210; // -> -1.14083
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 6.012345; // -> -.2776630
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 6.305555; // a little over 2pi -> .02237342
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 9.350123; // -> -.07479396
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -0.785398164; // -pi/4 -> -1
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -1.57079633; // -pi/2 -> 312012480.5 (really +INF)
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -3.14159265; // -pi -> 0
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -1.11111; // -> -2.01996
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -2.0000; // -> 2.1850
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -3.010203; // -> .1321510
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -4.567890; // -> -6.872231
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -5.43210; // -> 1.14083
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -6.012345; // -> .2776630
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -6.305555; // a little over -2pi -> -.02237342
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = -9.350123; // -> 0.07479396
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 0.00000000; // -> 0
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 0.50000000; // -> 0.54630249
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 112.; // -> -1.95
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    x = 112000.; // -> -1.31243
    "x = <<x>>, tan(x) = <<x.tangent()>>\n";

    "\b";

    /*
     *   Radian/degree conversions 
     */

    x = 90.0000;
    "x = <<x>>, d2r = <<x.degreesToRadians()>>\n";

    x = 180.000;
    "x = <<x>>, d2r = <<x.degreesToRadians()>>\n";

    x = 270.000;
    "x = <<x>>, d2r = <<x.degreesToRadians()>>\n";

    x = 360.000;
    "x = <<x>>, d2r = <<x.degreesToRadians()>>\n";

    x = 0.00000;
    "x = <<x>>, d2r = <<x.degreesToRadians()>>\n";

    x = -90.0000;
    "x = <<x>>, d2r = <<x.degreesToRadians()>>\n";

    x = -180.000;
    "x = <<x>>, d2r = <<x.degreesToRadians()>>\n";

    x = 3.14159265;
    "x = <<x>>, r2d = <<x.radiansToDegrees()>>\n";

    x = -3.14159265;
    "x = <<x>>, r2d = <<x.radiansToDegrees()>>\n";

    x = 0.00000;
    "x = <<x>>, r2d = <<x.radiansToDegrees()>>\n";

    x = 1.57079633;
    "x = <<x>>, r2d = <<x.radiansToDegrees()>>\n";

    x = -1.57079633;
    "x = <<x>>, r2d = <<x.radiansToDegrees()>>\n";

    "\b";

    /*
     *   arcsin/arccos 
     */
    x = 0.0000000;  // 0, 1.57079633
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = 0.50000000;  // 0.52359878, 1.04719755
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = -0.50000000;  // -.52359878, 2.09439510
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = 0.54030231;  // 0.57079633, 1
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = -0.54030231;  // -0.57079633, 2.14159266
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = 0.70710678;  // 0.78539816, 0.78539816
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = 0.84147098;  // 1, 0.57079633
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = 0.9000000;  // 1.11976952, .45102681
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = 0.9999990;  // 1.56938211, .00141421
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = -0.9999990;  // -1.56938211, 3.14017844
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = 1.0000000;  // 1.57079633, 0
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    x = -1.0000000;  // -1.57079633, 3.14159265
    "x = <<x>>, asin = <<x.arcsine()>>, acos = <<x.arccosine()>>\n";

    "\b";

    /*
     *   arctan 
     */
    x = 0.0000000;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = 1.2345678e-4;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = -1.2345678e-4;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = 0.2500000;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = -0.2500000;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = 1.0000000;
    "x = <<x>>, atan = <<x.arctangent()>>, atan(1)*4 =
        <<x.arctangent() * 4>>\n";

    x = -1.0000000;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = 3.14159265;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = -3.14159265;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = 123.45678;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = -123.45678;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = 9876.54321;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    x = -9876.54321;
    "x = <<x>>, atan = <<x.arctangent()>>\n";

    "\b";

    /*
     *   square root 
     */
    x = 1234.5678;
    "x = <<x>>, sqrt = <<x.sqrt()>>\n";

    x = 12345.678;
    "x = <<x>>, sqrt = <<x.sqrt()>>\n";

    x = 987.65432;
    "x = <<x>>, sqrt = <<x.sqrt()>>\n";

    x = 9876.5432;
    "x = <<x>>, sqrt = <<x.sqrt()>>\n";

    x = 2.0000000;
    "x = <<x>>, sqrt = <<x.sqrt()>>\n";

    x = 0.20000000;
    "x = <<x>>, sqrt = <<x.sqrt()>>\n";

    "\b";

    /*
     *   log 
     */
    x = 3.04050607e-10;
    "x = <<x.formatString(10, BignumExp)>>, ln = <<x.logE()>>\n";

    x = 0.50267771;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 0.750750750;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 0.999888777;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 1.00000000;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 1.00000123;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 1.99999999;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 2.00000001;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 11.7512345;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 1234567890.;
    "x = <<x>>, ln = <<x.logE()>>\n";

    x = 9.8765432e50;
    "x = <<x.formatString(10, BignumExp)>>, ln = <<x.logE()>>\n";

    "\b";

    /*
     *   exp 
     */
    x = 5.000000e-17;
    "x = <<x.formatString(10, BignumExp)>>, exp = <<x.expE()>>\n";

    x = -5.000000e-17;
    "x = <<x.formatString(10, BignumExp)>>, exp = <<x.expE()>>\n";

    x = .000500000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = -.000500000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = .500000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = -.500000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = 1.00000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = -1.00000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = 1.50000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = -1.50000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = 2.30000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = -2.30000;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = 15.00000;
    "x = <<x>>, exp = <<x.expE().formatString(10, BignumExp)>>\n";

    x = -15.0000;
    "x = <<x>>, exp = <<x.expE().formatString(10, BignumExp)>>\n";

    x = 150.000000;
    "x = <<x>>, exp = <<x.expE().formatString(10, BignumExp)>>\n";

    x = -150.00000;
    "x = <<x>>, exp = <<x.expE().formatString(10, BignumExp)>>\n";

    x = 1500.;
    "x = <<x>>, exp = <<x.expE()>>\n";

    x = -1500.;
    "x = <<x>>, exp = <<x.expE()>>\n";

    "\b";

    /*
     *   log10 tests 
     */
    x = 0.00001000;
    "x = <<x>>, log10 = <<x.log10()>>\n";

    x = 0.10000000;
    "x = <<x>>, log10 = <<x.log10()>>\n";

    x = 1.00000000;
    "x = <<x>>, log10 = <<x.log10()>>\n";

    x = 10.0000000;
    "x = <<x>>, log10 = <<x.log10()>>\n";

    x = 1.000000e23;
    "x = <<x.formatString(10, BignumExp)>>, log10 = <<x.log10()>>\n";

    x = 1234567890.;
    "x = <<x>>, log10 = <<x.log10()>>\n";

    x = 345676543.;
    "x = <<x>>, log10 = <<x.log10()>>\n";

    "\b";

    /*
     *   power tests 
     */
    x = 5.123456;
    y = 7.890123;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = 52.123456;
    y = -7.890123;
    "x = <<x>>, y = <<y>>,
        x^y = <<x.raiseToPower(y).formatString(10, BignumExp)>>\n";

    x = -3.123456;
    y = 7.;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = -5.123456;
    y = -7.;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = -3.123456;
    y = 6.;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = -5.123456;
    y = -6.;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = 0.00005234991;
    y = 0.00006781234;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = 0.000000;
    y = 1.000000;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = 12.000000;
    y = 0.000000;
    "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n";

    x = 0.000000;
    y = 0.000000;
    try { "x = <<x>>, y = <<y>>, x^y = <<x.raiseToPower(y)>>\n"; }
    catch (RuntimeError err) { "error: <<err.exceptionMessage>>\n"; }

    "\b";

    /*
     *   hyperbolic sine, cosine, and tangent 
     */

    x = 0.0000000;
    "x = <<x>>, sinh=<<x.sinh()>>, cosh=<<x.cosh()>>, tahn=<<x.tanh()>>\n";

    x = 1.0000000;
    "x = <<x>>, sinh=<<x.sinh()>>, cosh=<<x.cosh()>>, tahn=<<x.tanh()>>\n";

    x = -1.0000000;
    "x = <<x>>, sinh=<<x.sinh()>>, cosh=<<x.cosh()>>, tahn=<<x.tanh()>>\n";

    x = 2.5300000;
    "x = <<x>>, sinh=<<x.sinh()>>, cosh=<<x.cosh()>>, tahn=<<x.tanh()>>\n";

    x = -2.5300000;
    "x = <<x>>, sinh=<<x.sinh()>>, cosh=<<x.cosh()>>, tahn=<<x.tanh()>>\n";

    "\b";

    /*
     *   min/max 
     */
    x = 1.000;
    y = -3.000;
    z = 2.000;
    "x = <<x>>, y = <<y>>, z = <<z>>, min = <<min(x, y, z)>>,
        max = <<max(x, y, z)>>\n";
    "(x,y,z,-11,7): min = <<min(x, y, z, -11, 7)>>,
        max = <<max(x, y, z, -11, 7)>>\n";
    "max(z, y, x) = <<max(z, y, x)>>, max(y, x, z) = <<max(y, x, z)>>\n";
    "min(z, y, x) = <<min(z, y, x)>>, min(y, x, z) = <<min(y, x, z)>>\n";

    "\b";
}

bool(val)
{
    if (val)
        "true";
    else
        "nil";
}

preinit()
{
}

