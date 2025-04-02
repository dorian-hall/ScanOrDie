using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
public class Strikes : MonoBehaviour
{
    [SerializeField] Image[] strikes;
    int strikeCount = 0;
    public static Strikes instance;
    private void Start()
    {
        instance = this;
    }

    public bool CountStrike()
    {
        strikes[strikeCount].color = Color.red;
        strikeCount++;
        return strikeCount > strikes.Length-1;
    }
}
