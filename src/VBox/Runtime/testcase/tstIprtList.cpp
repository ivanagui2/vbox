/* $Id$ */
/** @file
 * IPRT Testcase - iprt::list.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/cpp/list.h>

#include <iprt/cpp/ministring.h>
#include <iprt/test.h>
#include <iprt/rand.h>

/*******************************************************************************
*   Test Data                                                                  *
*******************************************************************************/
static const char *gs_apcszTestStrings[] =
{
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
    "Vestibulum non turpis vel metus pellentesque tincidunt at id massa.",
    "Cras quis erat sed nulla ullamcorper molestie.",
    "Mauris ac elit turpis, id pulvinar diam.",
    "Nulla quis dolor dolor, in ultrices diam.",
    "Vivamus ac quam non ipsum vehicula tempor ac ac arcu.",
    "Aenean posuere lacus blandit erat semper eu iaculis ante eleifend.",
    "Donec quis quam a lacus interdum sollicitudin quis eu est.",
    "Morbi sed nisi a arcu commodo convallis.",
    "Aenean molestie condimentum velit, non mattis magna ultricies quis.",
    "Nulla id velit at mauris gravida mattis.",
    "Phasellus viverra velit eu urna semper in porta arcu sollicitudin.",
    "Pellentesque consequat turpis et tortor hendrerit id tempor ipsum lacinia.",
    "Cras iaculis nulla quis risus pulvinar eget tempor lectus placerat.",
    "Nullam in nulla sed sapien euismod euismod.",
    "Morbi in tortor at magna sagittis fermentum ut eu nunc.",
    "Nulla vitae ante sit amet dui molestie sagittis lacinia quis tellus.",
    "Proin iaculis lorem ultricies metus bibendum tincidunt.",
    "Sed gravida purus id risus sollicitudin ac porta orci vestibulum.",
    "Duis quis purus non ligula consectetur cursus eu interdum erat.",
    "Nullam non nunc in elit volutpat tempor in nec metus.",
    "Aliquam id purus eget enim luctus molestie.",
    "Sed id elit nec elit luctus scelerisque.",
    "Suspendisse viverra leo non ligula congue ac luctus nisl vulputate.",
    "Nulla dignissim lobortis nunc, eu tempus ipsum luctus sed.",
    "Integer vel lacus lacus, quis condimentum felis.",
    "Nulla ut lacus ac lacus gravida ultrices id sed ipsum.",
    "Etiam non purus ut augue fermentum consequat.",
    "Nam sit amet eros quis nibh blandit lacinia non posuere lectus.",
    "Sed sit amet ipsum et dolor sagittis facilisis.",
    "Ut congue nisi lacus, vel ultrices est.",
    "Donec vel erat ut justo hendrerit sodales eu eget libero.",
    "Integer a ipsum ac nunc eleifend congue convallis a urna.",
    "Sed vel eros eu lectus imperdiet vehicula.",
    "Vivamus eget turpis sed erat dapibus varius eget eu nulla.",
    "Nam id nulla non elit eleifend commodo sed ac est.",
    "Integer pulvinar dolor sodales velit pulvinar et facilisis eros scelerisque.",
    "Ut mattis arcu ut libero imperdiet in rhoncus augue sodales.",
    "Ut luctus turpis ligula, id dapibus felis.",
    "Nullam sit amet sapien eget tellus hendrerit vestibulum eget in odio.",
    "Phasellus non orci vitae mi placerat semper.",
    "Quisque pharetra aliquet velit, quis tempor magna porttitor nec.",
    "Praesent porta neque felis, vehicula facilisis odio.",
    "Maecenas ultricies ipsum eu velit laoreet faucibus.",
    "Mauris et nunc leo, et euismod quam.",
    "Phasellus a felis et justo fringilla lacinia.",
    "Vestibulum eget augue ante, ac viverra neque.",
    "Mauris pellentesque ligula quis metus elementum venenatis.",
    "Curabitur eu neque tellus, non porta sapien.",
    "Ut mattis metus id enim aliquam laoreet et sed tortor.",
    "Aenean quis nulla vitae nulla auctor lobortis a egestas turpis.",
    "Praesent vitae ante a urna porta placerat non nec eros.",
    "Donec quis neque eros, placerat adipiscing turpis.",
    "Cras sit amet sapien risus, quis euismod arcu.",
    "Integer volutpat massa eros, ac gravida mi.",
    "Nunc vitae nunc sagittis diam vulputate suscipit.",
    "Suspendisse quis mauris bibendum mauris aliquet pulvinar.",
    "Donec volutpat vestibulum ligula, eget interdum tortor malesuada sit amet.",
    "Mauris hendrerit dui non nibh varius sit amet fringilla orci pretium.",
    "Phasellus a quam tellus, auctor lacinia sapien.",
    "Sed dapibus leo vitae neque faucibus id porttitor sapien ultricies.",
    "Maecenas euismod elit nec tortor sagittis pretium.",
    "Ut tincidunt risus at erat fermentum sit amet molestie ante lacinia.",
    "Nulla non leo nec lacus sollicitudin lobortis a a nisl.",
    "Nunc vulputate erat vel libero elementum a interdum turpis malesuada.",
    "Morbi id libero turpis, a lobortis dolor.",
    "Donec vehicula imperdiet lorem, non pretium nulla tempus ut.",
    "Morbi lacinia massa id nunc tempus in blandit risus blandit.",
    "Sed feugiat orci id ipsum suscipit quis fringilla enim rutrum.",
    "Mauris suscipit lobortis urna, vel dictum justo iaculis ac.",
    "In rhoncus lectus tristique nunc blandit gravida placerat turpis rutrum.",
    "Aliquam pellentesque ornare justo, sed hendrerit metus mattis a.",
    "Nam aliquet lorem congue nisl blandit posuere.",
    "Sed lobortis interdum ipsum, ac cursus erat lacinia in.",
    "Maecenas vel tortor vel lorem facilisis interdum.",
    "Aenean porttitor massa enim, eget dignissim est.",
    "Nullam id libero lacus, mattis feugiat risus.",
    "Fusce et dolor at eros ornare auctor malesuada vel ipsum.",
    "Donec at massa sit amet lorem pellentesque interdum at ac lacus.",
    "Praesent suscipit velit at justo suscipit eu vestibulum ligula interdum.",
    "Aenean id justo nulla, vitae vulputate diam.",
    "Fusce pellentesque leo quis orci pulvinar at pellentesque tellus dictum.",
    "Ut facilisis purus at enim varius vulputate.",
    "Donec malesuada bibendum sapien, sed pretium nisi cursus quis.",
    "Mauris porttitor diam ut sapien pretium egestas.",
    "Vestibulum ut justo eu libero semper convallis vitae et velit.",
    "Quisque eleifend dapibus ligula, eu tincidunt massa rutrum at.",
    "Sed euismod diam eget enim suscipit dictum.",
    "Mauris fermentum orci eu nunc venenatis in sollicitudin tellus vestibulum.",
    "Vivamus faucibus consequat turpis, lobortis vehicula lectus gravida eget.",
    "Curabitur eu erat eu mi interdum scelerisque.",
    "Morbi consequat molestie nulla, imperdiet elementum augue sagittis vel.",
    "Sed ullamcorper velit suscipit arcu egestas quis commodo est hendrerit.",
    "Proin vitae velit ut enim sollicitudin ultrices.",
    "Curabitur posuere euismod lacus, sed volutpat erat adipiscing sit amet.",
    "Cras sit amet sem lorem, in cursus augue.",
    "Sed fermentum ultricies orci, quis hendrerit risus imperdiet et.",
    "Proin nec arcu interdum ipsum molestie vestibulum.",
    "Nulla quis quam non sem pretium scelerisque et eu velit.",
    "Donec eu tellus nisl, ac vehicula tortor."
};

/*******************************************************************************
*   Tests                                                                      *
*******************************************************************************/
template<typename T1, typename T2>
static void test(const char* pcszDesc, T2 aTestData[], size_t cSize)
{
    RTTestISubF("%s with size of %u (items=%u)", pcszDesc, sizeof(T1), cSize);

    /*
     * Construction
     */

    /* Create a test list */
    iprt::list<T1> testList;

    RTTESTI_CHECK(testList.isEmpty());
    RTTESTI_CHECK(testList.size()     == 0);
    RTTESTI_CHECK(testList.capacity() == iprt::list<T1>::DefaultCapacity);

    /*
     * Adding
     */

    /* Add the second half of the test data */
    size_t cAdded = 1;
    /* Start adding the second half of our test list */
    for (size_t i = cSize / 2; i < cSize; ++i, ++cAdded)
    {
        testList.append(aTestData[i]);
        RTTESTI_CHECK_RETV(testList.size()    == cAdded);
        RTTESTI_CHECK(testList.at(0)          == aTestData[cSize / 2]);
        RTTESTI_CHECK(testList[0]             == aTestData[cSize / 2]);
        RTTESTI_CHECK(testList.first()        == aTestData[cSize / 2]);
        RTTESTI_CHECK(testList.at(cAdded - 1) == aTestData[i]);
        RTTESTI_CHECK(testList[cAdded - 1]    == aTestData[i]);
        RTTESTI_CHECK(testList.last()         == aTestData[i]);
    }
    /* Check that all is correctly appended. */
    RTTESTI_CHECK_RETV(testList.size()        == cSize / 2);
    RTTESTI_CHECK_RETV(testList.isEmpty()     == false);
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList.at(i) == aTestData[cSize / 2 + i]);
    /* Start prepending the first half of our test list. Iterate reverse to get
     * the correct sorting back. */
    for (size_t i = cSize / 2; i > 0; --i, ++cAdded)
    {
        testList.prepend(aTestData[i - 1]);
        RTTESTI_CHECK_RETV(testList.size()    == cAdded);
        RTTESTI_CHECK(testList.at(0)          == aTestData[i - 1]);
        RTTESTI_CHECK(testList[0]             == aTestData[i - 1]);
        RTTESTI_CHECK(testList.first()        == aTestData[i - 1]);
        RTTESTI_CHECK(testList.at(cAdded - 1) == aTestData[cSize - 1]);
        RTTESTI_CHECK(testList[cAdded - 1]    == aTestData[cSize - 1]);
        RTTESTI_CHECK(testList.last()         == aTestData[cSize - 1]);
    }
    /* Check that all is correctly prepended. */
    RTTESTI_CHECK_RETV(testList.size()        == cSize);
    RTTESTI_CHECK_RETV(testList.isEmpty()     == false);
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList.at(i) == aTestData[i]);

    /*
     * Copy operator
     */
    iprt::list<T1> testList2(testList);
    /* Check that all is correctly appended. */
    RTTESTI_CHECK_RETV(testList2.size() == cSize);
    for (size_t i = 0; i < testList2.size(); ++i)
        RTTESTI_CHECK(testList2.at(i) == aTestData[i]);

    /*
     * "=" operator
     */
    iprt::list<T1> testList3;
    testList3 = testList;
    /* Check that all is correctly appended. */
    RTTESTI_CHECK_RETV(testList3.size() == cSize);
    for (size_t i = 0; i < testList3.size(); ++i)
        RTTESTI_CHECK(testList3.at(i) == aTestData[i]);

    /*
     * Append list
     */
    testList2.append(testList3);
    /* Check that all is correctly appended. */
    RTTESTI_CHECK_RETV(testList2.size() == cSize * 2);
    for (size_t i = 0; i < testList2.size(); ++i)
        RTTESTI_CHECK(testList2.at(i) == aTestData[i % cSize]);

    /*
     * Prepend list
     */
    testList2.prepend(testList3);
    /* Check that all is correctly appended. */
    RTTESTI_CHECK_RETV(testList2.size() == cSize * 3);
    for (size_t i = 0; i < testList2.size(); ++i)
        RTTESTI_CHECK(testList2.at(i) == aTestData[i % cSize]);

    /*
     * "value" method
     */
    for (size_t i = 0; i < testList2.size(); ++i)
        RTTESTI_CHECK(testList2.value(i)       == aTestData[i % cSize]);
    for (size_t i = 0; i < testList2.size(); ++i)
        RTTESTI_CHECK(testList2.value(i, T1()) == aTestData[i % cSize]);
    RTTESTI_CHECK(testList2.value(testList2.size() + 1) == T1());       /* Invalid index */
    RTTESTI_CHECK(testList2.value(testList2.size() + 1, T1()) == T1()); /* Invalid index */

    /*
     * operator[] (reading)
     */
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList[i] == aTestData[i]);
    /*
     * operator[] (writing)
     *
     * Replace with inverted array.
     */
    for (size_t i = 0; i < cSize; ++i)
        testList[i] = aTestData[cSize - i - 1];
    RTTESTI_CHECK_RETV(testList.size() == cSize);
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList[i] == aTestData[cSize - i - 1]);

    /*
     * Replace
     *
     * Replace with inverted array (Must be original array when finished).
     */
    for (size_t i = 0; i < cSize; ++i)
        testList.replace(i, aTestData[i]);
    RTTESTI_CHECK_RETV(testList.size() == cSize);
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList[i] == aTestData[i]);

    /*
     * Removing
     */

    /* Remove Range */
    testList2.removeRange(cSize, cSize * 2);
    RTTESTI_CHECK_RETV(testList2.size() == cSize * 2);
    for (size_t i = 0; i < testList2.size(); ++i)
        RTTESTI_CHECK(testList2.at(i) == aTestData[i % cSize]);

    /* Remove the first half (reverse) */
    size_t cRemoved = 1;
    for (size_t i = cSize / 2; i > 0; --i, ++cRemoved)
    {
        testList.removeAt(i - 1);
        RTTESTI_CHECK_RETV(testList.size() == cSize - cRemoved);
    }
    RTTESTI_CHECK_RETV(testList.size() == cSize / 2);
    /* Check that all is correctly removed and only the second part of the list
     * is still there. */
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList.at(i) == aTestData[cSize / 2 + i]);

    /*
     * setCapacitiy
     */
    testList.setCapacity(cSize * 5);
    RTTESTI_CHECK(testList.capacity()  == cSize * 5);
    RTTESTI_CHECK_RETV(testList.size() == cSize / 2);
    /* As the capacity just increased, we should still have all entries from
     * the previous list. */
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList.at(i) == aTestData[cSize / 2 + i]);
    /* Decrease the capacity so it will be smaller than the count of items in
     * the list. The list should be shrink automatically, but the remaining
     * items should be still valid. */
    testList.setCapacity(cSize / 4);
    RTTESTI_CHECK_RETV(testList.size() == cSize / 4);
    RTTESTI_CHECK(testList.capacity()  == cSize / 4);
    for (size_t i = 0; i < testList.size(); ++i)
        RTTESTI_CHECK(testList.at(i) == aTestData[cSize / 2 + i]);

    /* Clear all */
    testList.clear();
    RTTESTI_CHECK_RETV(testList.isEmpty());
    RTTESTI_CHECK_RETV(testList.size() == 0);
    RTTESTI_CHECK(testList.capacity()  == iprt::list<T1>::DefaultCapacity);
}

int main()
{
    /* How many integer test items should be created. */
    static const size_t s_cTestCount = 1000;

    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstIprtList", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /* Some host info. */
    RTTestIPrintf(RTTESTLVL_ALWAYS, "sizeof(void*)=%d", sizeof(void*));

    /*
     * The tests.
     */

    /*
     * Native types.
     */
    uint8_t au8TestInts[s_cTestCount];
    for (size_t i = 0; i < RT_ELEMENTS(au8TestInts); ++i)
        au8TestInts[i] = (float)RTRandU32() / UINT32_MAX * UINT8_MAX;
    test<uint8_t, uint8_t>("Native type", au8TestInts, RT_ELEMENTS(au8TestInts));
    uint16_t au16TestInts[s_cTestCount];
    for (size_t i = 0; i < RT_ELEMENTS(au16TestInts); ++i)
        au16TestInts[i] = (float)RTRandU32() / UINT32_MAX * UINT16_MAX;
    test<uint16_t, uint16_t>("Native type", au16TestInts, RT_ELEMENTS(au16TestInts));
    uint32_t au32TestInts[s_cTestCount];
    for (size_t i = 0; i < RT_ELEMENTS(au32TestInts); ++i)
        au32TestInts[i] = RTRandU32();
    test<uint32_t, uint32_t>("Native type", au32TestInts, RT_ELEMENTS(au32TestInts));
    /*
     * Specialized type.
     */
    uint64_t au64TestInts[s_cTestCount];
    for (size_t i = 0; i < RT_ELEMENTS(au64TestInts); ++i)
        au64TestInts[i] = RTRandU64();
    test<uint64_t, uint64_t>("Specialized type", au64TestInts, RT_ELEMENTS(au64TestInts));
    /*
     * Big size type (translate to internal pointer list).
     */
    test<iprt::MiniString, const char*>("Class type", gs_apcszTestStrings, RT_ELEMENTS(gs_apcszTestStrings));

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}
