using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Bomb : MonoBehaviour
{
    public bool Defused = false;

    private void Start()
    {
        Coursor.Instance.bombs.Add(transform);
    }

    private void OnDestroy()
    {
        if (!Defused)
        {
            AudioManager.instance.Explosion?.Post(AudioManager.instance.gameObject);
            Strikes.instance.CountStrike();
            Strikes.instance.CountStrike();
        }
        else
        {
            AudioManager.instance.Defused?.Post(AudioManager.instance.gameObject);
   
        }
        Coursor.Instance.bombs.Remove(transform);
    }
}
