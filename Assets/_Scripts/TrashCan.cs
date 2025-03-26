using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class TrashCan : MonoBehaviour
{
    private void OnTriggerEnter(Collider other)
    {
        if (other.TryGetComponent<Bomb>(out Bomb bomb)) bomb.Defused = true;
        else AudioManager.instance.WrongObject?.Post(AudioManager.instance.gameObject);
        Destroy(other.gameObject);
    }
}
