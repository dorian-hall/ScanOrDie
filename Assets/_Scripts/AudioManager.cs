using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class AudioManager : MonoBehaviour
{
    public AK.Wwise.Event Explosion;
    public AK.Wwise.Event WrongObject;
    public AK.Wwise.Event Defused;

     
    public static AudioManager instance;
    private void Awake()
    {
        instance = this;
    }
}
