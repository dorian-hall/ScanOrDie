using System;
using System.Collections;
using System.Collections.Generic;
using DG.Tweening;
using UnityEngine;

public class Mover : MonoBehaviour
{
    [SerializeField] List<GameObject> Crates = new List<GameObject>();
    [SerializeField] Transform target;
    [SerializeField] Ease ease;
    [SerializeField] float duration;
    // Start is called before the first frame update
    void Start()
    {
        Move();
    }

    async void Move()
    {
        for (int i = 0; i < Crates.Count; i++)
        {
            GameObject create = Instantiate(Crates[i],transform.position, Quaternion.identity,transform);
            await create.transform.DOMove(target.position, duration).SetEase(ease).AsyncWaitForCompletion();
            Destroy(create.gameObject);
        }
    }

    private void OnTriggerStay(Collider other)
    {
        Debug.Log("Hello World");
       // other.transform.position = transform.position + (Vector3.right * Time.fixedDeltaTime);
    }

    // Update is called once per frame
    void Update()
    {
        
    }
}
