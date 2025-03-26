using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class Bomb : MonoBehaviour
{
    public bool Defused = false;
    
    private void OnDestroy()
    {
        if (!Defused) Debug.Log("Explode");
        else Debug.Log("Defused");
    }
}
