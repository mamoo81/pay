/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "TestValue.h"
#include <BitcoinValue.h>
#include <FloweePay.h>

void TestValue::init()
{
    FloweePay::instance()->setUnit(FloweePay::BCH);
}

void TestValue::basics()
{
    BitcoinValue testObject;
    testObject.setRealValue(8790.9); // We only use the whole
    QCOMPARE(testObject.value(), 8790);

    testObject.setEnteredString("1.1945");
    QCOMPARE(testObject.value(), 119450000);
    QCOMPARE(testObject.cursorPos(), 6);
    testObject.insertNumber('1');
    QCOMPARE(testObject.value(), 119451000);
    QCOMPARE(testObject.cursorPos(), 7);
    testObject.insertNumber('2');
    QCOMPARE(testObject.value(), 119451200);
    QCOMPARE(testObject.cursorPos(), 8);
    testObject.insertNumber('4');
    QCOMPARE(testObject.value(), 119451240);
    QCOMPARE(testObject.cursorPos(), 9);
    testObject.insertNumber('9');
    QCOMPARE(testObject.value(), 119451249);
    QCOMPARE(testObject.cursorPos(), 10);
    testObject.insertNumber('4'); // no effect
    QCOMPARE(testObject.value(), 119451249);
    QCOMPARE(testObject.cursorPos(), 10);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 119451240);
    QCOMPARE(testObject.cursorPos(), 9);
    testObject.insertNumber('4');
    QCOMPARE(testObject.value(), 119451244);
    QCOMPARE(testObject.cursorPos(), 10);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 119451240);
    QCOMPARE(testObject.cursorPos(), 9);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 119451200);
    QCOMPARE(testObject.cursorPos(), 8);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 119451000);
    QCOMPARE(testObject.cursorPos(), 7);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 119450000);
    QCOMPARE(testObject.cursorPos(), 6);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 119400000);
    QCOMPARE(testObject.cursorPos(), 5);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 119000000);
    QCOMPARE(testObject.cursorPos(), 4);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 110000000);
    QCOMPARE(testObject.cursorPos(), 3);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 100000000);
    QCOMPARE(testObject.cursorPos(), 2);
    testObject.backspacePressed(); // removes the 'dot'
    QCOMPARE(testObject.value(), 100000000);
    QCOMPARE(testObject.cursorPos(), 1);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 0);
    QCOMPARE(testObject.cursorPos(), 0);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 0);
    QCOMPARE(testObject.cursorPos(), 0);
    testObject.addSeparator();
    QCOMPARE(testObject.value(), 0);
    QCOMPARE(testObject.cursorPos(), 2);
    testObject.addSeparator();
    QCOMPARE(testObject.cursorPos(), 2);
    testObject.insertNumber('6');
    QCOMPARE(testObject.value(), 60000000);
    QCOMPARE(testObject.cursorPos(), 3);

    /* when changing unit, do not change the typed text, just the interpreted value */
    FloweePay::instance()->setUnit(FloweePay::MilliBCH);
    QCOMPARE(testObject.value(), 60000);

    testObject.setEnteredString("0");
    QCOMPARE(testObject.cursorPos(), 1);
    testObject.insertNumber('0');
    QCOMPARE(testObject.value(), 0);
    QCOMPARE(testObject.cursorPos(), 1);
    testObject.insertNumber('0');
    QCOMPARE(testObject.value(), 0);
    QCOMPARE(testObject.cursorPos(), 1);
}

void TestValue::insertAtCursor()
{
    BitcoinValue testObject;
    testObject.setEnteredString("156.1");
    QCOMPARE(testObject.value(), 15610000000);
    QCOMPARE(testObject.cursorPos(), 5);
    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 4);
    testObject.insertNumber('4');
    QCOMPARE(testObject.value(), 15641000000);
    QCOMPARE(testObject.cursorPos(), 5);
    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 4);
    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 3);
    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 2);
    QCOMPARE(testObject.value(), 15641000000);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 1641000000);
    QCOMPARE(testObject.cursorPos(), 1);
    testObject.moveRight();
    QCOMPARE(testObject.cursorPos(), 2);
    testObject.moveRight();
    QCOMPARE(testObject.cursorPos(), 3);
    testObject.backspacePressed();
    QCOMPARE(testObject.value(), 164100000000);
    QCOMPARE(testObject.cursorPos(), 2);

    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 1);
    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 0);
    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 0);
    testObject.moveRight();
    QCOMPARE(testObject.cursorPos(), 1);
    testObject.moveRight();
    QCOMPARE(testObject.cursorPos(), 2);
    testObject.moveRight();
    QCOMPARE(testObject.cursorPos(), 3);
    testObject.moveRight();
    QCOMPARE(testObject.cursorPos(), 4);
    testObject.moveRight();
    QCOMPARE(testObject.cursorPos(), 4);
    testObject.backspacePressed();
    QCOMPARE(testObject.cursorPos(), 3);
    QCOMPARE(testObject.value(), 16400000000);

    testObject.moveLeft();
    QCOMPARE(testObject.cursorPos(), 2);
    testObject.addSeparator();
    QCOMPARE(testObject.cursorPos(), 3);
    QCOMPARE(testObject.value(), 1640000000);
}

void TestValue::fiatValues()
{
    BitcoinValue testObject;
    testObject.setMaxFractionalDigits(2);
    // this sets it to a fiat mode, like the Euro

    testObject.setEnteredString("4.5");
    QCOMPARE(testObject.cursorPos(), 3);
    QCOMPARE(testObject.value(), 450);
    testObject.insertNumber('7');
    QCOMPARE(testObject.cursorPos(), 4);
    QCOMPARE(testObject.value(), 457);
    testObject.insertNumber('5');
    QCOMPARE(testObject.cursorPos(), 4);
    QCOMPARE(testObject.value(), 457);
}

QTEST_MAIN(TestValue)
