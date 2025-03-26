using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Bomb : MonoBehaviour
{
    public bool Defused = false;
    
    private void OnDestroy()
    {
        if (!Defused) AudioManager.instance.Explosion?.Post(AudioManager.instance.gameObject);
        else AudioManager.instance.Defused?.Post(AudioManager.instance.gameObject);
    }
}
