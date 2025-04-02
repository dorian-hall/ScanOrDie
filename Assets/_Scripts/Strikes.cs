using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;
public class Strikes : MonoBehaviour
{
    [SerializeField] Image[] strikes;
    public int strikeCount = 0;
    public static Strikes instance;
    private void Start()
    {
        instance = this;
    }

    public bool CountStrike()
    {
        if(strikeCount< strikes.Length) strikes[strikeCount].color = Color.red;
        strikeCount++;
        return strikeCount > strikes.Length-1;
    }
}
