using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

public class TrashCan : MonoBehaviour
{
    private void OnTriggerEnter(Collider other)
    {
        if (other.TryGetComponent<Bomb>(out Bomb bomb)) bomb.Defused = true;
        else Debug.Log("Not A Bomb");
        Destroy(other.gameObject);
    }
}
